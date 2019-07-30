/* rggladewindow.cc
 *
 * Copyright (c) 2003 Michael Vogt
 *
 * Author: Michael Vogt <mvo@debian.irg>
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
#include <apt-pkg/fileutl.h>
#include <cassert>
#include "i18n.h"
#include "rggladewindow.h"


/*  the convention is that the top window is named:
   "window_$windowname" in your glade-source 
*/
RGGladeWindow::RGGladeWindow(RGWindow *parent, string name, string mainName)
{
   //std::cout << "RGGladeWindow::RGGladeWindow(parent,name)" << endl;

   _busyCursor = gdk_cursor_new(GDK_WATCH);

   // for development
   gchar *filename = NULL;
   gchar *local_filename = NULL;
   gchar *main_widget = NULL;

   filename = g_strdup_printf("window_%s.glade", name.c_str());
   local_filename = g_strdup_printf("glade/%s", filename);
   if (mainName.empty())
      main_widget = g_strdup_printf("window_%s", name.c_str());
   else
      main_widget = g_strdup_printf("window_%s", mainName.c_str());
   if (FileExists(local_filename)) {
      _gladeXML = glade_xml_new(local_filename, main_widget, NULL);
   } else {
      g_free(filename);
      filename =
         g_strdup_printf(SYNAPTIC_GLADEDIR "window_%s.glade", name.c_str());
      _gladeXML = glade_xml_new(filename, main_widget, NULL);
   }
   assert(_gladeXML);
   _win = glade_xml_get_widget(_gladeXML, main_widget);
   assert(_win);

   if(parent != NULL) 
      gtk_window_set_transient_for(GTK_WINDOW(_win), 
				   GTK_WINDOW(parent->window()));

   gtk_window_set_position(GTK_WINDOW(_win),
			   GTK_WIN_POS_CENTER_ON_PARENT);

   g_free(filename);
   g_free(local_filename);
   g_free(main_widget);

   //gtk_window_set_title(GTK_WINDOW(_win), (char *)name.c_str());

   gtk_object_set_data(GTK_OBJECT(_win), "me", this);
   gtk_signal_connect(GTK_OBJECT(_win), "delete-event",
                      (GtkSignalFunc) windowCloseCallback, this);
   _topBox = NULL;

   // honor foreign parent windows (to make embedding easy)
   int id = _config->FindI("Volatile::ParentWindowId", -1);
   if (id > 0) {
      GdkWindow *win = gdk_window_foreign_new(id);
      if(win) {
	 gtk_widget_realize(_win);
	 gdk_window_set_transient_for(GDK_WINDOW(_win->window), win);
      }
   }
   // if we have no parent, don't skip the taskbar hint
   if(_config->FindB("Volatile::HideMainwindow",false) && id < 0)
   {
      gtk_window_set_skip_taskbar_hint(GTK_WINDOW(_win), FALSE);
      gtk_window_set_urgency_hint(GTK_WINDOW(_win), TRUE);
   }

}

bool RGGladeWindow::setLabel(const char *widget_name, const char *value)
{
   GtkWidget *widget = glade_xml_get_widget(_gladeXML, widget_name);
   if (widget == NULL) {
      cout << "widget == NULL with: " << widget_name << endl;
      return false;
   }

   const gchar *utf8value=utf8(value);

   if (!utf8value)
      utf8value = _("N/A");
   gtk_label_set_label(GTK_LABEL(widget), utf8value);
   return true;
}

bool RGGladeWindow::setLabel(const char *widget_name, const long value)
{
   string strVal;
   GtkWidget *widget = glade_xml_get_widget(_gladeXML, widget_name);
   if (widget == NULL) {
      cout << "widget == NULL with: " << widget_name << endl;
      return false;
   }
   // we can never have values of zero or less
   if (value <= 0)
      // TRANSLATORS: this is a abbreviation for "not applicable" (on forms)
      // happens when e.g. a package has no installed version (or no
      // downloadable version)
      strVal = _("N/A");
   else
      strVal = SizeToStr(value);
   gtk_label_set_label(GTK_LABEL(widget), utf8(strVal.c_str()));
   return true;
}

bool RGGladeWindow::setTreeList(const char *widget_name, vector<string> values,
				bool use_markup)
{
   char *type;
   string strVal;
   GtkWidget *widget = glade_xml_get_widget(_gladeXML, widget_name);
   if (widget == NULL) {
      cout << "widget == NULL with: " << widget_name << endl;
      return false;
   }
   // create column (if needed)
   if(gtk_tree_view_get_column(GTK_TREE_VIEW(widget), 0) == NULL) {

      // cell renderer
      GtkCellRenderer *renderer;
      renderer = gtk_cell_renderer_text_new();

      if(use_markup)
	 type = "markup";
      else
	 type = "text";
      GtkTreeViewColumn *column;
      column = gtk_tree_view_column_new_with_attributes("SubView",
							renderer,
							type, 0, 
							NULL);
      gtk_tree_view_append_column(GTK_TREE_VIEW(widget), column);
   }

   // store stuff
   GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
   GtkTreeIter iter;
   for(unsigned int i=0;i<values.size();i++) {
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, 0, utf8(values[i].c_str()), -1);
   }
   gtk_tree_view_set_model(GTK_TREE_VIEW(widget), GTK_TREE_MODEL(store));
   return true;
}


bool RGGladeWindow::setTextView(const char *widget_name, 
				     const char* value, 
				     bool use_headline)
{
   GtkTextIter start,end;

   GtkWidget *view = glade_xml_get_widget(_gladeXML, widget_name);
   if (view == NULL) {
      cout << "textview == NULL with: " << widget_name << endl;
      return false;
   }

   GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
   const gchar *utf8value = utf8(value);
   if(!utf8value)
      utf8value = _("N/A");
   gtk_text_buffer_set_text (buffer, utf8value, -1);

   if(use_headline) {
      GtkTextTagTable *tag_table = gtk_text_buffer_get_tag_table(buffer);
      if(gtk_text_tag_table_lookup(tag_table, "bold") == NULL) {
	 gtk_text_buffer_create_tag(buffer, "bold", 
				    "weight", PANGO_WEIGHT_BOLD,
				    "scale", 1.1, 
				    NULL);
      }
      gtk_text_buffer_get_iter_at_offset(buffer, &start, 0);
      
      gtk_text_buffer_get_iter_at_offset(buffer, &end, 0);
      gtk_text_iter_forward_line(&end);

      gtk_text_buffer_apply_tag_by_name(buffer, "bold", &start, &end);
   }

   gtk_text_buffer_get_iter_at_offset(buffer, &start, 0);
   gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(view), &start,0,FALSE,0,0);
   return true;
}

bool RGGladeWindow::setPixmap(const char *widget_name, GdkPixbuf *value)
{
   GtkWidget *pix = glade_xml_get_widget(_gladeXML, widget_name);
   if (pix == NULL) {
      cout << "textview == NULL with: " << widget_name << endl;
      return false;
   }
   gtk_image_set_from_pixbuf(GTK_IMAGE(pix), value);
   
   return true;
}

void RGGladeWindow::setBusyCursor(bool flag) 
{
   if(flag) {
      if(GTK_WIDGET_VISIBLE(_win))
	 gdk_window_set_cursor(window()->window, _busyCursor);
#if GTK_CHECK_VERSION(2,4,0)
      // if we don't iterate here, the busy cursor is not shown
      while (gtk_events_pending())
	 gtk_main_iteration();
#endif
   } else {
      if(GTK_WIDGET_VISIBLE(_win))
	 gdk_window_set_cursor(window()->window, NULL);
   }
}
