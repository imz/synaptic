/* rgpkgdetails.cc - show details of a pkg
 * 
 * Copyright (c) 2004 Michael Vogt
 *
 * Author: Michael Vogt <mvo@debian.org>
 *         Gustavo Niemeyer <niemeyer@conectiva.com>
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

#include <cassert>
#include "rgwindow.h"
#include "rgmainwindow.h"
#include "rgpkgdetails.h"
#include "rggladewindow.h"
#include "rpackage.h"
#include "sections_trans.h"



RGPkgDetailsWindow::RGPkgDetailsWindow(RGWindow *parent)
   : RGGladeWindow(parent, "details")
{
   glade_xml_signal_connect_data(_gladeXML,
				 "on_button_close_clicked",
				 G_CALLBACK(cbCloseClicked),
				 this); 
}

void RGPkgDetailsWindow::cbCloseClicked(GtkWidget *self, void *data)
{
   RGPkgDetailsWindow *me = static_cast<RGPkgDetailsWindow*>(data);

   me->hide();
}

vector<string> 
RGPkgDetailsWindow::formatDepInformation(const vector<DepInformation> &deps)
{
   vector<string> depStrings;
   string depStr;
   
   for(unsigned int i=0;i<deps.size();i++) {
      depStr="";

      // type in bold (Depends, PreDepends)
      depStr += string("<b>") + _(DepTypeStr[deps[i].type]) + string(":</b> ");

      // virutal is italic
      if(deps[i].isVirtual) {
	 depStr += string("<i>") + deps[i].name + string("</i>");
      } else {
	 // is real pkg
	 depStr += deps[i].name;
	 // additional version information
	 if(deps[i].version != NULL) {
	    gchar *s = g_markup_escape_text(deps[i].versionComp,-1);
	    depStr += string(" (") + s + deps[i].version + string(")");
	    g_free(s);
	 }
      }

      // this is for or-ed dependencies (make them one-line)
      while(deps[i].isOr) {
	 depStr += " | ";
	 // next dep
	 i++;
	 depStr += deps[i].name;
	 // additional version information
	 if(deps[i].version != NULL) {
	    gchar *s = g_markup_escape_text(deps[i].versionComp,-1);
	    depStr += string(" (") + s + deps[i].version + string(")");
	    g_free(s);
	 }
      }
      
      depStrings.push_back(depStr);
   }
   return depStrings;
}

void RGPkgDetailsWindow::fillInValues(RGGladeWindow *me, RPackage *pkg,
				      bool setTitle)
{
   assert(me!=NULL);

   // TRANSLATORS: Title of the package properties dialog 
   //              %s is the name of the package
   if(setTitle) {
      gchar *str = g_strdup_printf(_("%s Properties"),pkg->name());
      me->setTitle(str);
      g_free(str);
   }

   char *pkg_summary = g_strdup_printf("%s\n%s",
					     pkg->name(), pkg->summary());
   me->setTextView("textview_pkgcommon", pkg_summary, true);
   g_free(pkg_summary);

   me->setLabel("label_maintainer", pkg->maintainer());
   me->setPixmap("image_state", RGPackageStatus::pkgStatus.getPixbuf(pkg));
   me->setLabel("label_state", RGPackageStatus::pkgStatus.getLongStatusString(pkg));
   me->setLabel("label_priority", pkg->priority());
   me->setLabel("label_section", trans_section(pkg->section()).c_str());
   me->setLabel("label_installed_version", pkg->installedVersion());
   me->setLabel("label_installed_size", pkg->installedSize());

   me->setLabel("label_latest_version", pkg->availableVersion());
   me->setLabel("label_latest_size", pkg->availableInstalledSize());
   me->setLabel("label_latest_download_size", pkg->availablePackageSize());

   // format description nicely and use emblems
   GtkWidget *textview;
   GtkTextBuffer *buf;
   GtkTextIter it, start, end;
   GtkWidget *emblem;
   const gchar *s;
   static GtkTooltips *tips = gtk_tooltips_new ();

   textview = glade_xml_get_widget(me->getGladeXML(), "text_descr");
   assert(textview);
   buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
   // clear old buffer
   gtk_text_buffer_get_start_iter(buf, &start);
   gtk_text_buffer_get_end_iter(buf, &end);
   gtk_text_buffer_delete(buf, &start, &end);
   // create bold tag
   GtkTextTagTable *tag_table = gtk_text_buffer_get_tag_table(buf);
   if(gtk_text_tag_table_lookup(tag_table, "bold") == NULL) {
      gtk_text_buffer_create_tag(buf, "bold", 
				 "weight", PANGO_WEIGHT_BOLD,
				 "scale", 1.1, 
				 NULL);
   }
   // set summary
   s = utf8(pkg->summary());
   gtk_text_buffer_get_start_iter(buf, &it);
   gtk_text_buffer_insert(buf, &it, s, -1);
   gtk_text_buffer_get_start_iter(buf, &start);
   gtk_text_buffer_apply_tag_by_name(buf, "bold", &start, &it);
   // set emblems 
   GdkPixbuf *pixbuf = RGPackageStatus::pkgStatus.getSupportedPix(pkg);
   if(pixbuf != NULL) {
      // insert space
      gtk_text_buffer_insert(buf, &it, " ", 1);
      // make image
      emblem = gtk_image_new_from_pixbuf(pixbuf);
      gtk_image_set_pixel_size(GTK_IMAGE(emblem), 16);
      // set eventbox and tooltip
      GtkWidget *event = gtk_event_box_new();
      GtkStyle *style = gtk_widget_get_style(textview);
      gtk_widget_modify_bg(event, GTK_STATE_NORMAL, 
			   &style->base[GTK_STATE_NORMAL]);
      gtk_container_add(GTK_CONTAINER(event), emblem);
      gtk_tooltips_set_tip(tips, event, _("This application is supported by the distribution"), "");
      // create anchor
      GtkTextChildAnchor *anchor = gtk_text_buffer_create_child_anchor(buf, &it);
      gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(textview), event, anchor);
      gtk_widget_show_all(event);
   }
   // show the rest of the description
   gtk_text_buffer_insert(buf, &it, "\n", 1);
   s = utf8(pkg->description());
   gtk_text_buffer_insert(buf, &it, s, -1);
   

   // build dependency lists
   vector<DepInformation> deps;
   deps = pkg->enumDeps();
   me->setTreeList("treeview_deplist", formatDepInformation(deps), true);
   
   // canidateVersion
   deps = pkg->enumDeps(true);
   me->setTreeList("treeview_availdep_list", formatDepInformation(deps), true);

   // rdepends Version
   deps = pkg->enumRDeps();
   me->setTreeList("treeview_rdeps", formatDepInformation(deps), true);
   
   // provides
   me->setTreeList("treeview_provides", pkg->provides());


   // file list
#ifndef HAVE_RPM
   gtk_widget_show(glade_xml_get_widget(me->getGladeXML(),
					"scrolledwindow_filelist"));
   me->setTextView("textview_files", pkg->installedFiles());
#endif

   // versions
   gchar *str;
   vector<string> list;
   vector<pair<string,string> > versions = pkg->getAvailableVersions();
   for(size_t i=0;i<versions.size();++i) {
      // TRANSLATORS: this the format of the available versions in 
      // the "Properties/Available versions" window
      // e.g. "0.56 (unstable)"
      //      "0.53.4 (testing)"
      str = g_strdup_printf(_("%s (%s)"), 
			    versions[i].first.c_str(), 
			    versions[i].second.c_str());
      list.push_back(str);
      g_free(str);
   }
   me->setTreeList("treeview_versions", list);

   glade_xml_signal_connect_data(me->getGladeXML(), 
				 "on_optionmenu_depends_changed",
				 G_CALLBACK(cbDependsMenuChanged), me);
}

void RGPkgDetailsWindow::cbDependsMenuChanged(GtkWidget *self, void *data)
{
   RGPkgDetailsWindow *me = (RGPkgDetailsWindow*)data;

   int nr =  gtk_option_menu_get_history(GTK_OPTION_MENU(self));
   GtkWidget *notebook = glade_xml_get_widget(me->_gladeXML, 
					      "notebook_dep_tab");
   assert(notebook);
   gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), nr);
}

RGPkgDetailsWindow::~RGPkgDetailsWindow()
{
   
}
