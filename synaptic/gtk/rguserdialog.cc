/* rguserdialog.cc
 *
 * Copyright (c) 2000, 2001 Conectiva S/A
 *               2003 Michael Vogt
 *
 * Author: Alfredo K. Kojima <kojima@conectiva.com.br>
 *         Michael Vogt <mvo@debian.org>
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

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <assert.h>
#include "i18n.h"
#include "rguserdialog.h"
#include "rgmisc.h"

static void actionResponse(GtkDialog *dialog, gint id, gpointer user_data)
{
   GtkResponseType *res = (GtkResponseType *) user_data;
   *res = (GtkResponseType) id;
}

bool RGUserDialog::message(const char *msg,
                           RUserDialog::DialogType dialog,
                           RUserDialog::ButtonsType buttons, bool defres)
{
   GtkWidget *dia;
   GtkResponseType res;
   GtkMessageType gtkmessage;
   GtkButtonsType gtkbuttons;

   switch (dialog) {
      case RUserDialog::DialogInfo:
         gtkmessage = GTK_MESSAGE_INFO;
         gtkbuttons = GTK_BUTTONS_OK;
         break;
      case RUserDialog::DialogWarning:
         gtkmessage = GTK_MESSAGE_WARNING;
         gtkbuttons = GTK_BUTTONS_OK;
         break;
      case RUserDialog::DialogError:
         gtkmessage = GTK_MESSAGE_ERROR;
         gtkbuttons = GTK_BUTTONS_OK;
         break;
      case RUserDialog::DialogQuestion:
         gtkmessage = GTK_MESSAGE_QUESTION;
         gtkbuttons = GTK_BUTTONS_YES_NO;
         break;
   }

   switch (buttons) {
      case RUserDialog::ButtonsDefault:
         break;
      case RUserDialog::ButtonsOk:
         gtkbuttons = GTK_BUTTONS_OK;
         break;
      case RUserDialog::ButtonsOkCancel:
         gtkbuttons = GTK_BUTTONS_OK_CANCEL;
         break;
      case RUserDialog::ButtonsYesNo:
         gtkbuttons = GTK_BUTTONS_YES_NO;
         break;
   }

   dia = gtk_message_dialog_new(GTK_WINDOW(_parentWindow),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                gtkmessage, gtkbuttons, "%s", utf8(msg));

   if (defres) {
      switch (buttons) {
         case RUserDialog::ButtonsOkCancel:
            gtk_dialog_set_default_response(GTK_DIALOG(dia), GTK_RESPONSE_OK);
            break;
         case RUserDialog::ButtonsYesNo:
            gtk_dialog_set_default_response(GTK_DIALOG(dia), GTK_RESPONSE_YES);
            break;
      }
   } else {
      switch (buttons) {
         case RUserDialog::ButtonsOkCancel:
            gtk_dialog_set_default_response(GTK_DIALOG(dia),
                                            GTK_RESPONSE_CANCEL);
            break;
         case RUserDialog::ButtonsYesNo:
            gtk_dialog_set_default_response(GTK_DIALOG(dia), GTK_RESPONSE_NO);
            break;
      }
   }

   g_signal_connect(GTK_OBJECT(dia), "response",
                    G_CALLBACK(actionResponse), (gpointer) & res);
   gtk_dialog_run(GTK_DIALOG(dia));
   gtk_widget_destroy(dia);
   return (res == GTK_RESPONSE_OK) || (res == GTK_RESPONSE_YES);
}

// RGGladeUserDialog
RGGladeUserDialog::RGGladeUserDialog(RGWindow *parent, const char *name)
{
   init(name);
   _parentWindow = parent->window();
}

bool RGGladeUserDialog::init(const char *name)
{
   gchar *filename = NULL;
   gchar *main_widget = NULL;

   //cout << "RGGladeUserDialog::RGGladeUserDialog()" << endl;

   filename = g_strdup_printf("dialog_%s.glade", name);
   main_widget = g_strdup_printf("dialog_%s", name);
   if (FileExists(filename)) {
      gladeXML = glade_xml_new(filename, main_widget, NULL);
   } else {
      g_free(filename);
      filename = g_strdup_printf(SYNAPTIC_GLADEDIR "dialog_%s.glade", name);
      gladeXML = glade_xml_new(filename, main_widget, NULL);
   }
   assert(gladeXML);
   _dialog = glade_xml_get_widget(gladeXML, main_widget);
   assert(_dialog);

   gtk_window_set_position(GTK_WINDOW(_dialog),
			   GTK_WIN_POS_CENTER_ON_PARENT);
   gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_dialog), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(_dialog), 
				GTK_WINDOW(_parentWindow));

   g_free(filename);
   g_free(main_widget);
}

int RGGladeUserDialog::run(const char *name,bool return_gtk_response)
{
   if(name != NULL)
      init(name);

   res = (GtkResponseType) gtk_dialog_run(GTK_DIALOG(_dialog));
   gtk_widget_hide(_dialog);

   if(return_gtk_response)
      return res;
   else
      return (res == GTK_RESPONSE_OK) || (res == GTK_RESPONSE_YES);
}


// vim:sts=4:sw=4
