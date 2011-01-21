/* rgdebinstallprogress.cc
 *
 * Copyright (c) 2004 Canonical
 *
 * Author: Michael Vogt <mvo@debian.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "config.h"

#ifdef WITH_DPKG_STATUSFD

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "rgmainwindow.h"
#include "gsynaptic.h"

#include "rgdebinstallprogress.h"
#include "rguserdialog.h"

#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <gtk/gtk.h>

#include <unistd.h>
#include <stdio.h>

#include <vte/vte.h>


#include "i18n.h"

// timeout in sec until the expander is expanded
static const int RGTERMINAL_TIMEOUT=60;

// removing
char* RGDebInstallProgress::remove_stages[NR_REMOVE_STAGES] = {
   "half-configured", 
   "half-installed", 
   "config-files"};
char* RGDebInstallProgress::remove_stages_translations[NR_REMOVE_STAGES] = {
   N_("Preparing for removal %s"),
   N_("Removing %s"),
   N_("Removed %s")};

// purging
char *RGDebInstallProgress::purge_stages[NR_PURGE_STAGES] = { 
   "half-configured",
   "half-installed", 
   "config-files", 
   "not-installed"};
char *RGDebInstallProgress::purge_stages_translations[NR_PURGE_STAGES] = { 
   N_("Preparing for removal %s"),
   N_("Removing with config %s"), 
   N_("Removed %s"), 
   N_("Removed with config %s")};

// purge only (for packages that are alreay removed)
char *RGDebInstallProgress::purge_only_stages[NR_PURGE_ONLY_STAGES] = { 
   "config-files", 
   "not-installed"};
char *RGDebInstallProgress::purge_only_stages_translations[NR_PURGE_ONLY_STAGES] = { 
   N_("Removing with config %s"), 
   N_("Removed with config %s")};

// install 
char *RGDebInstallProgress::install_stages[NR_INSTALL_STAGES] = { 
   "half-installed",
   "unpacked",
   "half-configured",
   "installed"};
char *RGDebInstallProgress::install_stages_translations[NR_INSTALL_STAGES] = { 
   N_("Preparing %s"),
   N_("Unpacking %s"),
   N_("Configuring %s"),
   N_("Installed %s")};

// update
char *RGDebInstallProgress::update_stages[NR_UPDATE_STAGES] = { 
   "unpack",
   "half-installed", 
   "unpacked",
   "half-configured",
   "installed"};
char *RGDebInstallProgress::update_stages_translations[NR_UPDATE_STAGES] = { 
   N_("Preparing %s"),
   N_("Installing %s"), 
   N_("Unpacking %s"),
   N_("Configuring %s"),
   N_("Installed %s")};

//reinstall
char *RGDebInstallProgress::reinstall_stages[NR_REINSTALL_STAGES] = { 
   "half-configured",
   "unpacked",
   "half-installed", 
   "unpacked",
   "half-configured",
   "installed" };
char *RGDebInstallProgress::reinstall_stages_translations[NR_REINSTALL_STAGES] = { 
   N_("Preparing %s"),
   N_("Unpacking %s"),
   N_("Installing %s"), 
   N_("Unpacking %s"),
   N_("Configuring %s"),
   N_("Installed %s") };


void RGDebInstallProgress::child_exited(VteReaper *vtereaper,
					gint child_pid, gint ret, 
					gpointer data)
{
   RGDebInstallProgress *me = (RGDebInstallProgress*)data;

   if(child_pid == me->_child_id) {
//        cout << "correct child exited" << endl;
//        cout << "waitpid returned: " << WEXITSTATUS(ret) << endl;
      me->res = (pkgPackageManager::OrderResult)WEXITSTATUS(ret);
      me->child_has_exited=true;
   }
}


ssize_t
write_fd(int fd, void *ptr, size_t nbytes, int sendfd)
{
        struct msghdr   msg;
        struct iovec    iov[1];

        union {
          struct cmsghdr        cm;
          char   control[CMSG_SPACE(sizeof(int))];
        } control_un;
        struct cmsghdr  *cmptr;

        msg.msg_control = control_un.control;
        msg.msg_controllen = sizeof(control_un.control);

        cmptr = CMSG_FIRSTHDR(&msg);
        cmptr->cmsg_len = CMSG_LEN(sizeof(int));
        cmptr->cmsg_level = SOL_SOCKET;
        cmptr->cmsg_type = SCM_RIGHTS;
        *((int *) CMSG_DATA(cmptr)) = sendfd;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;

        iov[0].iov_base = ptr;
        iov[0].iov_len = nbytes;
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        return(sendmsg(fd, &msg, 0));
}



ssize_t
read_fd(int fd, void *ptr, size_t nbytes, int *recvfd)
{
        struct msghdr   msg;
        struct iovec    iov[1];
        ssize_t  n;
        int newfd;

        union {
          struct cmsghdr        cm;
          char   control[CMSG_SPACE(sizeof(int))];
        } control_un;
        struct cmsghdr  *cmptr;

        msg.msg_control = control_un.control;
        msg.msg_controllen = sizeof(control_un.control);

        msg.msg_name = NULL;
        msg.msg_namelen = 0;

        iov[0].iov_base = ptr;
        iov[0].iov_len = nbytes;
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        if ( (n = recvmsg(fd, &msg, MSG_WAITALL)) <= 0)
                return(n);
        if ( (cmptr = CMSG_FIRSTHDR(&msg)) != NULL &&
            cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
	   if (cmptr->cmsg_level != SOL_SOCKET) {
	      perror("control level != SOL_SOCKET");
	      exit(1);
	   }
	   if (cmptr->cmsg_type != SCM_RIGHTS) {
	      perror("control type != SCM_RIGHTS");
	      exit(1);
	   }
                *recvfd = *((int *) CMSG_DATA(cmptr));
        } else
                *recvfd = -1;           /* descriptor was not passed */
        return(n);
}
/* end read_fd */

#define UNIXSTR_PATH "/var/run/synaptic.socket"

int ipc_send_fd(int fd)
{
   // open connection to server
   struct sockaddr_un servaddr;
   int serverfd = socket(AF_LOCAL, SOCK_STREAM, 0);
   bzero(&servaddr, sizeof(servaddr));
   servaddr.sun_family = AF_LOCAL;
   strcpy(servaddr.sun_path, UNIXSTR_PATH);

   // wait max 5s (5000 * 1000/1000000) for the server
   for(int i=0;i<5000;i++) {
      if(connect(serverfd, (struct sockaddr *)&servaddr, sizeof(servaddr))==0) 
	 break;
      usleep(1000);
   }
   // send fd to server
   write_fd(serverfd, (void*)"",1,fd);
   close(serverfd);
   return 0;
}

int ipc_recv_fd()
{
   int ret;

   // setup socket
   struct sockaddr_un servaddr,cliaddr;
   char c;
   int connfd=-1,fd;

   int listenfd = socket(AF_LOCAL, SOCK_STREAM, 0);
   fcntl(listenfd, F_SETFL, O_NONBLOCK);

   unlink(UNIXSTR_PATH);
   bzero(&servaddr, sizeof(servaddr));
   servaddr.sun_family = AF_LOCAL;
   strcpy(servaddr.sun_path, UNIXSTR_PATH);
   bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
   listen(listenfd, 1);

   // wait for connections
   socklen_t clilen = sizeof(cliaddr);

   // wait max 5s (5000 * 1000/1000000) for the client
   for(int i=0;i<5000 || connfd > 0;i++) {
      connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
      if(connfd > 0)
	 break;
      usleep(1000);
      RGFlushInterface();
   }
   // read_fd 
   read_fd(connfd, &c,1,&fd);

   close(connfd);
   close(listenfd);

   return fd;
}



void RGDebInstallProgress::conffile(gchar *conffile, gchar *status)
{
   string primary, secondary;
   gchar *m,*s,*p;
   GtkWidget *w;
   RGGladeUserDialog dia(this, "conffile");
   GladeXML *xml = dia.getGladeXML();

   p = g_strdup_printf(_("Replace configuration file\n'%s'?"),conffile);
   s = g_strdup_printf(_("The configuration file %s was modified (by "
			 "you or by a script). An updated version is shipped "
			 "in this package. If you want to keep your current "
			 "version say 'Keep'. Do you want to replace the "
			 "current file and install the new package "
			 "maintainers version? "),conffile);

   // setup dialog
   w = glade_xml_get_widget(xml, "label_message");
   m = g_strdup_printf("<span weight=\"bold\" size=\"larger\">%s </span> "
		       "\n\n%s", p, s);
   gtk_label_set_markup(GTK_LABEL(w), m);
   g_free(p);
   g_free(s);
   g_free(m);

   // diff stuff
   bool quot=false;
   int i=0;
   string orig_file, new_file;

   // FIXME: add some sanity checks here

   // go to first ' and read until the end
   for(;status[i] != '\'' || status[i] == 0;i++) 
      /*nothing*/
      ;
   i++;
   for(;status[i] != '\'' || status[i] == 0;i++) 
      orig_file.append(1,status[i]);
   i++;

   // same for second ' and read until the end
   for(;status[i] != '\'' || status[i] == 0;i++) 
      /*nothing*/
      ;
   i++;
   for(;status[i] != '\'' || status[i] == 0;i++) 
      new_file.append(1,status[i]);
   i++;
   //cout << "got:" << orig_file << new_file << endl;

   // read diff
   string diff;
   char buf[512];
   char *cmd = g_strdup_printf("/usr/bin/diff -u %s %s", orig_file.c_str(), new_file.c_str());
   FILE *f = popen(cmd,"r");
   while(fgets(buf,512,f) != NULL) {
      diff += utf8(buf);
   }
   pclose(f);
   g_free(cmd);

   // set into buffer
   GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(glade_xml_get_widget(xml,"textview_diff")));
   gtk_text_buffer_set_text(text_buffer,diff.c_str(),-1);

   int res = dia.run(NULL,true);
   if(res ==  GTK_RESPONSE_YES)
      vte_terminal_feed_child(VTE_TERMINAL(_term), "y\n",2);
   else
      vte_terminal_feed_child(VTE_TERMINAL(_term), "n\n",2);

   // update the "action" clock
   last_term_action = time(NULL);
}

void RGDebInstallProgress::startUpdate()
{
   child_has_exited=false;
   VteReaper* reaper = vte_reaper_get();
   g_signal_connect(G_OBJECT(reaper), "child-exited",
		    G_CALLBACK(child_exited),
		    this);

   // check if we run embedded
   int id = _config->FindI("Volatile::PlugProgressInto", -1);
   //cout << "Plug ID: " << id << endl;
   if (id > 0) {
      GtkWidget *vbox = glade_xml_get_widget(_gladeXML, "vbox_rgdebinstall_progress");
      _sock =  gtk_plug_new(id);
      gtk_widget_reparent(vbox, _sock);
      gtk_widget_show(_sock);
   } else {
      show();
   }
   RGFlushInterface();
}

void RGDebInstallProgress::cbCancel(GtkWidget *self, void *data)
{
   //FIXME: we can't activate this yet, it's way to heavy (sending KILL)
   //cout << "cbCancel: sending SIGKILL to child" << endl;
   RGDebInstallProgress *me = (RGDebInstallProgress*)data;
   //kill(me->_child_id, SIGINT);
   //kill(me->_child_id, SIGQUIT);
   kill(me->_child_id, SIGTERM);
   //kill(me->_child_id, SIGKILL);
   
}

void RGDebInstallProgress::cbClose(GtkWidget *self, void *data)
{
   //cout << "cbCancel: sending SIGKILL to child" << endl;
   RGDebInstallProgress *me = (RGDebInstallProgress*)data;
   me->_updateFinished = true;
}


void RGDebInstallProgress::expander_callback (GObject    *object,
					      GParamSpec *param_spec,
					      gpointer    user_data) 
{
   RGDebInstallProgress *me = (RGDebInstallProgress*)user_data;

   // this crap here is needed because VteTerminal does not like
   // it when run hidden. this workaround will scroll to the end of
   // the current buffer
   gtk_widget_realize(GTK_WIDGET(me->_term));
   GtkAdjustment *a = GTK_ADJUSTMENT (VTE_TERMINAL(me->_term)->adjustment);
   gtk_adjustment_set_value(a, a->upper - a->page_size);
   gtk_adjustment_value_changed(a);

   gtk_widget_grab_focus(me->_term);
}

bool RGDebInstallProgress::close()
{
   if(child_has_exited)
      cbClose(NULL, this);

   return TRUE;
}

RGDebInstallProgress::~RGDebInstallProgress()
{
   delete _userDialog;
}

RGDebInstallProgress::RGDebInstallProgress(RGMainWindow *main,
					   RPackageLister *lister)

   : RInstallProgress(), RGGladeWindow(main, "rgdebinstall_progress"),
     _totalActions(0), _progress(0), _sock(0), _userDialog(0)

{
   prepare(lister);
   setTitle(_("Applying Changes"));

   // make sure we try to get a graphical debconf
   putenv("DEBIAN_FRONTEND=gnome");
   putenv("APT_LISTCHANGES_FRONTEND=gtk");

   _startCounting = false;
   _label_status = glade_xml_get_widget(_gladeXML, "label_status");
   _pbarTotal = glade_xml_get_widget(_gladeXML, "progress_total");
   gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(_pbarTotal), 0.025);
   _autoClose = glade_xml_get_widget(_gladeXML, "checkbutton_auto_close");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_autoClose), 
				_config->FindB("Synaptic::closeZvt", false));
   //_image = glade_xml_get_widget(_gladeXML, "image");


   _term = vte_terminal_new();
   vte_terminal_set_size(VTE_TERMINAL(_term),80,23);
   GtkWidget *scrollbar = 
      gtk_vscrollbar_new (GTK_ADJUSTMENT (VTE_TERMINAL(_term)->adjustment));
   GTK_WIDGET_UNSET_FLAGS (scrollbar, GTK_CAN_FOCUS);
   vte_terminal_set_scrollback_lines(VTE_TERMINAL(_term), 10000);
   if(_config->FindB("Synaptic::useUserTerminalFont")) {
      char *s =(char*)_config->Find("Synaptic::TerminalFontName").c_str();
      vte_terminal_set_font_from_string(VTE_TERMINAL(_term), s);
   } else {
      vte_terminal_set_font_from_string(VTE_TERMINAL(_term), "monospace 8");
   }
   gtk_box_pack_start(GTK_BOX(glade_xml_get_widget(_gladeXML,"hbox_vte")), _term, TRUE, TRUE, 0);
   gtk_widget_show(_term);
   gtk_box_pack_end(GTK_BOX(glade_xml_get_widget(_gladeXML,"hbox_vte")), scrollbar, FALSE, FALSE, 0);
   gtk_widget_show(scrollbar);

   gtk_window_set_default_size(GTK_WINDOW(_win), 500, -1);

   GtkWidget *w = glade_xml_get_widget(_gladeXML, "expander_terminal");
   g_signal_connect(w, "notify::expanded",
		    G_CALLBACK(expander_callback), this);

   g_signal_connect(_term, "contents-changed",
		    G_CALLBACK(content_changed), this);

   glade_xml_signal_connect_data(_gladeXML, "on_button_cancel_clicked",
				 G_CALLBACK(cbCancel), this);
   glade_xml_signal_connect_data(_gladeXML, "on_button_close_clicked",
				 G_CALLBACK(cbClose), this);

   if(_userDialog == NULL)
      _userDialog = new RGUserDialog(this);

   // init the timer
   last_term_action = time(NULL);
}

void RGDebInstallProgress::content_changed(GObject *object, 
					   gpointer data)
{
   //cout << "RGDebInstallProgress::content_changed()" << endl;

   RGDebInstallProgress *me = (RGDebInstallProgress*)data;

   me->last_term_action = time(NULL);
}

void RGDebInstallProgress::updateInterface()
{
   char buf[2];
   static char line[1024] = "";
   int i=0;

   while (1) {

      // This algorithm should be improved (it's the same as the rpm one ;)
      int len = read(_childin, buf, 1);

      // nothing was read
      if(len < 1) 
	 break;

      // update the time we last saw some action
      last_term_action = time(NULL);

      if( buf[0] == '\n') {
// 	 cout << line << endl;
	 
	 gchar **split = g_strsplit(line, ":",4);
	 
	 gchar *s=NULL;
	 gchar *pkg = g_strstrip(split[1]);
	 gchar *status = g_strstrip(split[2]);
	 // major problem here, we got unexpected input. should _never_ happen
	 if(!(pkg && status))
	    continue;

	 // first check for errors and conf-file prompts
	 if(strstr(status, "error") != NULL) { 
	    // error from dpkg
	    s = g_strdup_printf(_("Error in package %s"), split[1]);
	    string err = split[1] + string(": ") + split[3];
	    _error->Error(err.c_str());
	 } else if(strstr(status, "conffile-prompt") != NULL) {
	    // conffile-request
	    //cout << split[2] << " " << split[3] << endl;
	    conffile(pkg, split[3]);
	 } else if(_actionsMap.count(pkg) == 0) {
	    // no known dpkg state (happens e.g if apt reports:
	    // /bin/sh: apt-listchanges: command-not-found
	    g_strfreev(split);
	    line[0] = 0;
	    continue;
	 } else {
	    _startCounting = true;

	    // then go on with the package stuff
	    char *next_stage_str = NULL;
	    int next_stage = _stagesMap[pkg];
	    // is a element is not found in the map, NULL is returned
	    // (this happens when dpkg does some work left from a previous
	    //  session (rare but happens))
	    
	    char **states = _actionsMap[pkg]; 
	    char **translations = _translationsMap[pkg]; 
	    if(states && translations) {
	       next_stage_str = states[next_stage];
// 	       cout << "waiting for: " << next_stage_str << endl;
	       if(next_stage_str && (strstr(status, next_stage_str) != NULL)) {
		  s = g_strdup_printf(_(translations[next_stage]), split[1]);
		  next_stage++;
		  _stagesMap[pkg] = next_stage;
		  _progress++;
	       }
	    }
	 }

	 // each package goes through various stages
	 float val = ((float)_progress)/((float)_totalActions);
// 	 cout << _progress << "/" << _totalActions << " = " << val << endl;
	 gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(_pbarTotal), val);
	 if(s!=NULL)
	    gtk_label_set(GTK_LABEL(_label_status),s);
	 
	 // clean-up
	 g_strfreev(split);
	 g_free(s);
	 line[0] = 0;
      } else {
	 buf[1] = 0;
	 strcat(line, buf);
      }      
   }

   time_t now = time(NULL);

   if(!_startCounting) {
      usleep(100000);
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR(_pbarTotal));
      // wait until we get the first message from apt
      last_term_action = now;
   }


   if ((now - last_term_action) > RGTERMINAL_TIMEOUT) {
      cout << "no statusfd changes/content updates in terminal for " 
	   << RGTERMINAL_TIMEOUT << "seconds" << endl;
      GtkWidget *w;
      w = glade_xml_get_widget(_gladeXML, "expander_terminal");
      gtk_expander_set_expanded(GTK_EXPANDER(w), TRUE);
      last_term_action = time(NULL);
   } 


   if (gtk_events_pending()) {
      while (gtk_events_pending())
         gtk_main_iteration();
   } else {
      usleep(5000);
   }
}

pkgPackageManager::OrderResult RGDebInstallProgress::start(RPackageManager *pm,
                                                       int numPackages,
                                                       int numPackagesTotal)
{
   void *dummy;
   pkgPackageManager::OrderResult res;
   int ret;

   res = pm->DoInstallPreFork();
   if (res == pkgPackageManager::Failed)
       return res;

   /*
    * This will make a pipe from where we can read child's output
    */
   _child_id = vte_terminal_forkpty(VTE_TERMINAL(_term),NULL,NULL,
				    false,false,false);
   if (_child_id == 0) {
      int fd[2];
      pipe(fd);
      ipc_send_fd(fd[0]); // send the read part of the pipe to the parent

#ifdef WITH_DPKG_STATUSFD
      res = pm->DoInstallPostFork(fd[1]);
#else
      res = pm->DoInstallPostFork();
#endif

      // dump errors into cerr (pass it to the parent process)	
      _error->DumpErrors();

      ::close(fd[0]);
      ::close(fd[1]);

      _exit(res);
   }
   _childin = ipc_recv_fd();

   if(_childin < 0) {
      // something _bad_ happend. so the terminal window and hope for the best
      GtkWidget *w = glade_xml_get_widget(_gladeXML, "expander_terminal");
      gtk_expander_set_expanded(GTK_EXPANDER(w), TRUE);
      gtk_widget_hide(_pbarTotal);
   }

   // make it nonblocking
   fcntl(_childin, F_SETFL, O_NONBLOCK);

   _donePackages = 0;
   _numPackages = numPackages;
   _numPackagesTotal = numPackagesTotal;

   startUpdate();
   while(!child_has_exited)
      updateInterface();

   finishUpdate();

   ::close(_childin);

   return res;
}



void RGDebInstallProgress::finishUpdate()
{
   if (_startCounting) {
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(_pbarTotal), 1.0);
   }
   RGFlushInterface();

   GtkWidget *_closeB = glade_xml_get_widget(_gladeXML, "button_close");
   gtk_widget_set_sensitive(_closeB, TRUE);

   bool autoClose= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_autoClose));
   if(res == 0) {
      gtk_widget_grab_focus(_closeB);
      if(autoClose)
	 _updateFinished = True;
   }

   string s = _config->Find("Volatile::InstallFinishedStr",
			    _("Changes applied"));
   gchar *msg = g_strdup_printf("<big><b>%s</b></big>\n%s", s.c_str(),
				_(getResultStr(res)));
   setTitle(_("Changes applied"));
   GtkWidget *l = glade_xml_get_widget(_gladeXML, "label_action");
   gtk_label_set_markup(GTK_LABEL(l), msg);
   g_free(msg);

   // hide progress and label
   gtk_widget_hide(_pbarTotal);
   gtk_widget_hide(_label_status);

   GtkWidget *img = glade_xml_get_widget(_gladeXML,"image_finished");
   switch(res) {
   case 0: // success
      gtk_image_set_from_file(GTK_IMAGE(img),
			      PACKAGE_DATA_DIR"/pixmaps/synaptic.png");
      break;
   case 1: // error
      gtk_image_set_from_stock(GTK_IMAGE(img), GTK_STOCK_DIALOG_ERROR,
			       GTK_ICON_SIZE_DIALOG);
      _userDialog->showErrors();
      break;
   case 2: // incomplete
      gtk_image_set_from_stock(GTK_IMAGE(img), GTK_STOCK_DIALOG_INFO,
			       GTK_ICON_SIZE_DIALOG);
      break;
   }
   gtk_widget_show(img);
   

   // wait for the user to click on "close"
   while(!_updateFinished && !autoClose) {
      autoClose= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_autoClose));
      while (gtk_events_pending())
	 gtk_main_iteration();
      usleep(5000);
   }

   // get the value again, it may have changed
   _config->Set("Synaptic::closeZvt", autoClose	? "true" : "false");

   // hide and finish
   if(_sock != NULL) {
      gtk_widget_destroy(_sock);
   } else {
      hide();
   }
}

void RGDebInstallProgress::prepare(RPackageLister *lister)
{
   //cout << "prepeare called" << endl;

   // build a meaningfull dialog
   int installed, broken, toInstall, toReInstall, toRemove;
   double sizeChange;
   gchar *p = "Should never be displayed, please report";
   string s = _config->Find("Volatile::InstallProgressStr",
			    _("The marked changes are now being applied. "
			      "This can take some time. Please wait."));
   lister->getStats(installed, broken, toInstall, toReInstall, 
		    toRemove, sizeChange);
   if(toRemove > 0 && toInstall > 0) 
      p = _("Installing and removing software");
   else if(toRemove > 0)
      p = _("Removing software");
   else if(toInstall > 0)
      p =  _("Installing software");

   gchar *msg = g_strdup_printf("<big><b>%s</b></big>\n\n%s", p, s.c_str());
   GtkWidget *l = glade_xml_get_widget(_gladeXML, "label_action");
   gtk_label_set_markup(GTK_LABEL(l), msg);
   g_free(msg);

   for (unsigned int row = 0; row < lister->packagesSize(); row++) {
      RPackage *pkg = lister->getPackage(row);
      int flags = pkg->getFlags();
      string name = pkg->name();

      if((flags & RPackage::FPurge)&&
	 ((flags & RPackage::FInstalled)||(flags&RPackage::FOutdated))){
	 _actionsMap.insert(pair<string,char**>(name, purge_stages));
	 _translationsMap.insert(pair<string,char**>(name, purge_stages_translations));
	 _stagesMap.insert(pair<string,int>(name, 0));
	 _totalActions += NR_PURGE_STAGES;
      } else if((flags & RPackage::FPurge)&& 
		(!(flags & RPackage::FInstalled)||(flags&RPackage::FOutdated))){
	 _actionsMap.insert(pair<string,char**>(name, purge_only_stages));
	 _translationsMap.insert(pair<string,char**>(name, purge_only_stages_translations));
	 _stagesMap.insert(pair<string,int>(name, 0));
	 _totalActions += NR_PURGE_ONLY_STAGES;
      } else if(flags & RPackage::FRemove) {
	 _actionsMap.insert(pair<string,char**>(name, remove_stages));
	 _translationsMap.insert(pair<string,char**>(name, remove_stages_translations));
	 _stagesMap.insert(pair<string,int>(name, 0));
	 _totalActions += NR_REMOVE_STAGES;
      } else if(flags & RPackage::FNewInstall) {
	 _actionsMap.insert(pair<string,char**>(name, install_stages));
	 _translationsMap.insert(pair<string,char**>(name, install_stages_translations));
	 _stagesMap.insert(pair<string,int>(name, 0));
	 _totalActions += NR_INSTALL_STAGES;
      } else if(flags & RPackage::FReInstall) {
	 _actionsMap.insert(pair<string,char**>(name, reinstall_stages));
	 _translationsMap.insert(pair<string,char**>(name, reinstall_stages_translations));
	 _stagesMap.insert(pair<string,int>(name, 0));
	 _totalActions += NR_REINSTALL_STAGES;
      } else if((flags & RPackage::FUpgrade)||(flags & RPackage::FDowngrade)) {
	 _actionsMap.insert(pair<string,char**>(name, update_stages));
	 _translationsMap.insert(pair<string,char**>(name, update_stages_translations));
	 _stagesMap.insert(pair<string,int>(name, 0));
	 _totalActions += NR_UPDATE_STAGES;
      }
   }
}

#endif


// vim:ts=3:sw=3:et
