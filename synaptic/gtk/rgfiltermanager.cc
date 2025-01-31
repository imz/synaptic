/* rgfiltermanager.cc
 *
 * Copyright (c) 2000, 2001 Conectiva S/A
 *               2002,2003 Michael Vogt <mvo@debian.org>
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


#include "config.h"
#include <stdio.h>
#include <cassert>
#include <cstring>
#include "rpackageview.h"
#include "rgfiltermanager.h"

#include "i18n.h"

RGFilterManagerWindow::RGFilterManagerWindow(RGWindow *win,
                                             RPackageViewFilter *filterview)
: RGGladeWindow(win, "filters"), _selectedPath(NULL),
  _selectedFilter(NULL), _filterview(filterview)
{
   setTitle(_("Filters"));

   _busyCursor = gdk_cursor_new(GDK_WATCH);

   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_filters_add_clicked",
                                 G_CALLBACK(addFilterAction), this);

   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_filters_remove_clicked",
                                 G_CALLBACK(removeFilterAction), this);

   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_ok_clicked",
                                 G_CALLBACK(okAction), this);

   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_cancel_clicked",
                                 G_CALLBACK(cancelAction), this);

   glade_xml_signal_connect_data(_gladeXML,
                                 "on_entry_pattern_text_changed",
                                 G_CALLBACK(patternChanged), this);
   glade_xml_signal_connect_data(_gladeXML,
                                 "on_optionmenu_pattern_do_changed",
                                 G_CALLBACK(patternChanged), this);
   glade_xml_signal_connect_data(_gladeXML,
                                 "on_optionmenu_pattern_what_changed",
                                 G_CALLBACK(patternChanged), this);
   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_pattern_new_clicked",
                                 G_CALLBACK(patternNew), this);
   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_pattern_delete_clicked",
                                 G_CALLBACK(patternDelete), this);

   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_status_select_all_clicked",
                                 G_CALLBACK(statusAllClicked), this);
   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_status_select_none_clicked",
                                 G_CALLBACK(statusNoneClicked), this);
   glade_xml_signal_connect_data(_gladeXML,
                                 "on_button_status_invert_clicked",
                                 G_CALLBACK(statusInvertClicked), this);

   glade_xml_signal_connect_data(_gladeXML,
                                 "on_entry_filters_changed",
                                 G_CALLBACK(filterNameChanged), this);

   gtk_signal_connect(GTK_OBJECT(_win), "delete_event",
                      GTK_SIGNAL_FUNC(deleteEventAction), this);


   _filterDetailsBox = glade_xml_get_widget(_gladeXML, "vbox_filter_details");
   assert(_filterDetailsBox);

   // filter list view
   _filterEntry = glade_xml_get_widget(_gladeXML, "entry_filters");
   _filterList = glade_xml_get_widget(_gladeXML, "treeview_filters");
   _filterListStore = gtk_list_store_new(N_COLUMNS,
                                         G_TYPE_STRING, G_TYPE_POINTER);
   GtkCellRenderer *renderer;
   GtkTreeViewColumn *column;

   renderer = gtk_cell_renderer_text_new();
   //g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
   column = gtk_tree_view_column_new_with_attributes("Name",
                                                     renderer,
                                                     "text", NAME_COLUMN,
                                                     NULL);
   gtk_tree_view_append_column(GTK_TREE_VIEW(_filterList), column);
   gtk_tree_view_set_model(GTK_TREE_VIEW(_filterList),
                           GTK_TREE_MODEL(_filterListStore));
   /* Setup the selection handler */
   GtkTreeSelection *select;
   select = gtk_tree_view_get_selection(GTK_TREE_VIEW(_filterList));
   gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
   g_signal_connect(G_OBJECT(select), "changed",
                    G_CALLBACK(selectAction), this);

   // section list
   const set<string> &sections = _filterview->getSections();
   _sectionList = glade_xml_get_widget(_gladeXML, "treeview_sections");
   _sectionListStore = gtk_list_store_new(SECTION_N_COLUMNS, G_TYPE_STRING);
   renderer = gtk_cell_renderer_text_new();
   column = gtk_tree_view_column_new_with_attributes("Name",
                                                     renderer,
                                                     "text", SECTION_COLUMN,
                                                     NULL);
   gtk_tree_view_append_column(GTK_TREE_VIEW(_sectionList), column);
   gtk_tree_view_set_model(GTK_TREE_VIEW(_sectionList),
                           GTK_TREE_MODEL(_sectionListStore));
   /* Setup the selection handler */
   select = gtk_tree_view_get_selection(GTK_TREE_VIEW(_sectionList));
   gtk_tree_selection_set_mode(select, GTK_SELECTION_MULTIPLE);

   // fill selections dialog
   GtkTreeIter iter;
   for (set<string>::iterator I=sections.begin(); I != sections.end(); I++) {
      gtk_list_store_append(_sectionListStore, &iter);
      gtk_list_store_set(_sectionListStore, &iter,
                         SECTION_COLUMN, (*I).c_str(), -1);
   }

   // build pkg status dialog
   char *s;
   for (int i = 0; i < NrOfStatusBits; i++) {
      s = g_strdup_printf("checkbutton_status%i", i + 1);
      _statusB[i] = glade_xml_get_widget(_gladeXML, s);
      //cout << "loaded: " << s << endl;
      assert(_statusB[i]);
#ifdef HAVE_RPM // hide debian only boxes
      if(i == 9 || i == 10)
	gtk_widget_hide(_statusB[i]);
#endif
      g_free(s);
   }

   // pattern stuff
   _patternList = glade_xml_get_widget(_gladeXML, "treeview_pattern");
   _patternListStore = gtk_list_store_new(PATTERN_N_COLUMNS,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING, G_TYPE_STRING);
   renderer = gtk_cell_renderer_text_new();
   column = gtk_tree_view_column_new_with_attributes(_("Field"),
                                                     renderer,
                                                     "text",
                                                     PATTERN_WHAT_COLUMN,
                                                     NULL);
   gtk_tree_view_append_column(GTK_TREE_VIEW(_patternList), column);

   column = gtk_tree_view_column_new_with_attributes(_("Operator"),
                                                     renderer,
                                                     "text", PATTERN_DO_COLUMN,
                                                     NULL);
   gtk_tree_view_append_column(GTK_TREE_VIEW(_patternList), column);

   renderer = gtk_cell_renderer_text_new();
   //g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
   column = gtk_tree_view_column_new_with_attributes(_("Pattern"),
                                                     renderer,
                                                     "text",
                                                     PATTERN_TEXT_COLUMN,
                                                     NULL);
   gtk_tree_view_append_column(GTK_TREE_VIEW(_patternList), column);

   gtk_tree_view_set_model(GTK_TREE_VIEW(_patternList),
                           GTK_TREE_MODEL(_patternListStore));
   select = gtk_tree_view_get_selection(GTK_TREE_VIEW(_patternList));
   gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
   g_signal_connect(G_OBJECT(select), "changed",
                    G_CALLBACK(patternSelectionChanged), this);

   // remove the debtags tab
#ifndef HAVE_DEBTAGS
   GtkWidget *notebook = glade_xml_get_widget(_gladeXML, "notebook_details");
   assert(notebook);
   //if(first_run)
   gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), 3);
#endif

   // set the details filter to not sesitive
   gtk_widget_set_sensitive(_filterDetailsBox, false);

   skipTaskbar(true);
}

void RGFilterManagerWindow::filterNameChanged(GObject *o, gpointer data)
{
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   //cout << "RGFilterManagerWindow::filterNameChanged()"<<endl;

   if (me->_selectedPath == NULL || me->_selectedFilter == NULL)
      return;

   const gchar *s = gtk_entry_get_text(GTK_ENTRY(me->_filterEntry));
   // test for empty filtername
   if (s == NULL || !strcmp(s, ""))
      return;
   me->_selectedFilter->setName(s);

   GtkTreeIter iter;
   if (gtk_tree_model_get_iter(GTK_TREE_MODEL(me->_filterListStore), &iter,
                               me->_selectedPath)) {
      gtk_list_store_set(me->_filterListStore, &iter, NAME_COLUMN, s, -1);
   }
}

void RGFilterManagerWindow::statusInvertClicked(GObject *o, gpointer data)
{
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   //cout << "RGFilterManagerWindow::statusInvertClicked()"<<endl;

   gboolean x;
   for (int i = 0; i < NrOfStatusBits; i++) {
      x = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(me->_statusB[i]));
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(me->_statusB[i]), !x);
   }
}


void RGFilterManagerWindow::statusAllClicked(GObject *o, gpointer data)
{
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   //cout << "RGFilterManagerWindow::statusAllClicked()"<<endl;

   for (int i = 0; i < NrOfStatusBits; i++) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(me->_statusB[i]), TRUE);
   }
}

void RGFilterManagerWindow::statusNoneClicked(GObject *o, gpointer data)
{
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   //cout << "RGFilterManagerWindow::statusNoneClicked()"<<endl;
   for (int i = 0; i < NrOfStatusBits; i++) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(me->_statusB[i]), FALSE);
   }
}


void RGFilterManagerWindow::patternNew(GObject *o, gpointer data)
{
   //cout << "void RGFilterManagerWindow::patternNew()" << endl;
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;

   me->setPatternRow(-1, false, (RPatternPackageFilter::DepType) 0, "");


}

void RGFilterManagerWindow::patternDelete(GObject *o, gpointer data)
{
   //cout << "void RGFilterManagerWindow::patternDelete()" << endl;
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;

   GtkTreeSelection *select;
   GtkTreeIter iter;
   GtkTreeModel *model;

   select = gtk_tree_view_get_selection(GTK_TREE_VIEW(me->_patternList));
   if (gtk_tree_selection_get_selected(select, &model, &iter)) {
      gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
   }
}


void RGFilterManagerWindow::patternChanged(GObject *o, gpointer data)
{
   RPatternPackageFilter::DepType type;
   bool exclude;
   int i;
   GtkWidget *item, *menu;


   //cout << "patternChanged" << endl;
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;

   GtkWidget *w = glade_xml_get_widget(me->_gladeXML, "entry_pattern_text");
   gtk_signal_handler_block_by_func(GTK_OBJECT(w),
                                    GTK_SIGNAL_FUNC(patternChanged), data);

   GtkWidget *menuDo = glade_xml_get_widget(me->_gladeXML,
                                            "optionmenu_pattern_do");
   menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(menuDo));
   item = gtk_menu_get_active(GTK_MENU(menu));
   if (strcmp("menuitem_excl", gtk_widget_get_name(item)) == 0)
      exclude = true;
   else
      exclude = false;

   GtkWidget *menuType = glade_xml_get_widget(me->_gladeXML,
                                              "optionmenu_pattern_what");
   menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(menuType));
   item = gtk_menu_get_active(GTK_MENU(menu));
   const gchar *name = gtk_widget_get_name(item);
   sscanf(name, "menuitem_type%i", &i);

   type = (RPatternPackageFilter::DepType) i;

   GtkWidget *patternEntry = glade_xml_get_widget(me->_gladeXML,
                                                  "entry_pattern_text");
   const gchar *str = gtk_entry_get_text(GTK_ENTRY(patternEntry));

   // get path
   GtkTreeSelection *selection;
   GtkTreeIter iter;
   GtkTreeModel *model;
   int *indices;
   selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(me->_patternList));
   if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
      GtkTreePath *path;
      path = gtk_tree_model_get_path(model, &iter);
      indices = gtk_tree_path_get_indices(path);
      // set pattern
      me->setPatternRow(indices[0], exclude, type, str);
      gtk_tree_path_free(path);
   }
   gtk_signal_handler_unblock_by_func(GTK_OBJECT(w),
                                      GTK_SIGNAL_FUNC(patternChanged), data);

}

void RGFilterManagerWindow::patternSelectionChanged(GtkTreeSelection *
                                                    selection, gpointer data)
{
   //cout << "RGFilterManagerWindow::patternSelectionChanged()" << endl;

   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   GtkTreeIter iter;
   GtkTreeModel *model;
   RPatternPackageFilter::DepType type;
   bool exclude;
   gchar *dopatt, *what, *text;

   if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
      gtk_widget_set_sensitive(glade_xml_get_widget
                               (me->_gladeXML, "hbox_pattern"), TRUE);
      gtk_tree_model_get(model, &iter, PATTERN_DO_COLUMN, &dopatt,
                         PATTERN_WHAT_COLUMN, &what, PATTERN_TEXT_COLUMN,
                         &text, -1);

      if (strcmp(dopatt, ActOptions[0]) == 0)
         exclude = false;
      else
         exclude = true;
      GtkWidget *doPattern = glade_xml_get_widget(me->_gladeXML,
                                                  "optionmenu_pattern_do");
      gtk_option_menu_set_history(GTK_OPTION_MENU(doPattern), exclude ? 1 : 0);

      GtkWidget *typePattern = glade_xml_get_widget(me->_gladeXML,
                                                    "optionmenu_pattern_what");
      for (int j = 0; DepOptions[j]; j++) {
         if (strcmp(what, _(DepOptions[j])) == 0) {
            type = (RPatternPackageFilter::DepType) j;
            break;
         }
      }
      gtk_option_menu_set_history(GTK_OPTION_MENU(typePattern), (int)type);

      GtkWidget *patternText = glade_xml_get_widget(me->_gladeXML,
                                                    "entry_pattern_text");
      gtk_entry_set_text(GTK_ENTRY(patternText), text);
      g_free(dopatt);
      g_free(what);
      g_free(text);
   } else {
      gtk_widget_set_sensitive(glade_xml_get_widget
                               (me->_gladeXML, "hbox_pattern"), FALSE);
   }
}

gint RGFilterManagerWindow::deleteEventAction(GtkWidget *widget,
                                              GdkEvent * event, gpointer data)
{
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   me->cancelAction(widget, data);

   return (TRUE);
}


void RGFilterManagerWindow::readFilters()
{
   _selectedFilter = NULL;
   
   vector<string> filters = _filterview->getFilterNames();
   GtkTreeIter iter;

   gtk_list_store_clear(_filterListStore);

   for (unsigned int i = 0; i < filters.size(); i++) {
      RFilter *filter = _filterview->findFilter(i);
      gtk_list_store_append(_filterListStore, &iter);
      gtk_list_store_set(_filterListStore, &iter,
                         NAME_COLUMN, filter->getName().c_str(),
                         FILTER_COLUMN, filter, -1);
   }

   GtkTreeSelection *selection;
   selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(_filterList));
   if (_selectedPath != NULL) {
      gtk_tree_selection_select_path(selection, _selectedPath);
      editFilter();
   }

   gtk_entry_set_text(GTK_ENTRY(_filterEntry), "");

   // save filter list (do a real copy, needed for cancel)
   _saveFilters.clear();
   for (guint i = 0; i < _filterview->nrOfFilters(); i++) {
      RFilter *filter = _filterview->findFilter(i);
      _saveFilters.push_back(new RFilter(*filter));
   }



   RGWindow::show();
}


void RGFilterManagerWindow::selectAction(GtkTreeSelection *selection,
                                         gpointer data)
{
   //cout << "selectAction"<<endl;
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   GtkTreeIter iter;
   GtkTreeModel *model;
   RFilter *filter;
   gchar *filtername;

   if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
      gtk_widget_set_sensitive(me->_filterDetailsBox, true);

      if (me->_selectedPath != NULL) {
         applyFilterAction(NULL, me);
         gtk_tree_path_free(me->_selectedPath);
      }

      gtk_tree_model_get(model, &iter,
                         NAME_COLUMN, &filtername, FILTER_COLUMN, &filter, -1);
      me->_selectedPath = gtk_tree_model_get_path(model, &iter);
      //cout << "path is " << gtk_tree_path_to_string(me->_selectedPath) << endl;
      me->_selectedFilter = filter;
      //cout << "You selected" << filter << endl;
      gtk_entry_set_text(GTK_ENTRY(me->_filterEntry), filtername);
      g_free(filtername);

      me->editFilter();

      // make sure that the pattern stuff is only available if something
      // is selected in the patternList
      GtkTreeSelection *select;
      select = gtk_tree_view_get_selection(GTK_TREE_VIEW(me->_patternList));
      gtk_tree_selection_unselect_all(select);
      gtk_widget_set_sensitive(glade_xml_get_widget
                               (me->_gladeXML, "hbox_pattern"), false);
   } else {
      gtk_widget_set_sensitive(me->_filterDetailsBox, false);
   }
}


// mvo: helper function 
GtkTreePath *RGFilterManagerWindow::treeview_find_path_from_text(GtkTreeModel *
                                                                 model,
                                                                 char *text)
{
   GtkTreeIter iter;
   GtkTreePath *path;
   int i = 0;
   char *s;

   if (!gtk_tree_model_get_iter_first(model, &iter))
      return NULL;

   do {
      gtk_tree_model_get(model, &iter, SECTION_COLUMN, &s, -1);
      if (s != NULL)
         if (strcmp(s, text) == 0) {
            path = gtk_tree_path_new();
            gtk_tree_path_append_index(path, i);
            return path;
         }
      g_free(s);
      i++;
   } while (gtk_tree_model_iter_next(model, &iter));

   return NULL;
}


void RGFilterManagerWindow::setSectionFilter(RSectionPackageFilter & f)
{
   //cout << "RGFilterEditor::setSectionFilter()"<<endl;
   GtkWidget *treeView;
   GtkTreeSelection *selection;
   GtkTreeModel *model;
   GtkTreePath *path;
   string section;

   treeView = glade_xml_get_widget(_gladeXML, "treeview_sections");
   model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeView));
   selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
   gtk_tree_selection_unselect_all(selection);

   for (int i=0; i < f.count(); i++) {
      section = f.section(i);
      path = treeview_find_path_from_text(model, (char *)section.c_str());
      if (path != NULL) {
         gtk_tree_selection_select_path(selection, path);
         gtk_tree_path_free(path);
      }
   }

   GtkWidget *_inclGB = glade_xml_get_widget(_gladeXML, "radiobutton_incl");
   assert(_inclGB);
   GtkWidget *_exclGB = glade_xml_get_widget(_gladeXML, "radiobutton_excl");
   assert(_exclGB);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_inclGB), FALSE);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_exclGB), FALSE);
   if (f.inclusive()) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_inclGB), TRUE);
   } else {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_exclGB), TRUE);
   }
}



void RGFilterManagerWindow::setStatusFilter(RStatusPackageFilter & f)
{
   //cout << "RGFilterEditor::setStatusFilter(RStatusPackageFilter &f)"<<endl;

   int i;
   int type = f.status();

   for (i = 0; i < NrOfStatusBits; i++) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_statusB[i]),
                                   (type & StatusMasks[i]) ? TRUE : FALSE);
   }

}

bool RGFilterManagerWindow::setPatternRow(int row,
                                          bool exclude,
                                          RPatternPackageFilter::DepType type,
                                          const string &pattern)
{
   GtkTreeIter iter;
   const char *array[3];

   array[0] = ActOptions[exclude ? 1 : 0];
   array[1] = DepOptions[(int)type];
   array[2] = pattern.c_str();

   if (row < 0) {
      gtk_list_store_append(GTK_LIST_STORE(_patternListStore), &iter);
      gtk_list_store_set(GTK_LIST_STORE(_patternListStore), &iter,
                         0, array[0], 1, _(array[1]), 2, array[2], -1);
      GtkTreeSelection *select =
         gtk_tree_view_get_selection(GTK_TREE_VIEW(_patternList));
      gtk_tree_selection_select_iter(select, &iter);
   } else {
      GtkTreePath *path = gtk_tree_path_new();
      gtk_tree_path_prepend_index(path, row);
      if (gtk_tree_model_get_iter(GTK_TREE_MODEL(_patternListStore),
                                  &iter, path)) {
         gtk_list_store_set(GTK_LIST_STORE(_patternListStore),
                            &iter, 0, array[0], 1, _(array[1]), 2, 
			    array[2], -1);
      }
      gtk_tree_path_free(path);
   }

   return true;
}


void RGFilterManagerWindow::setPatternFilter(RPatternPackageFilter &f)
{
   //cout << "RGFilterEditor::setPatternFilter()"<<endl;

   gtk_list_store_clear(_patternListStore);
   for (int i = 0; i < f.count(); i++) {
      RPatternPackageFilter::DepType type;
      string pattern, s;
      bool exclude;
      f.getPattern(i, type, pattern, exclude);
      setPatternRow(-1, exclude, type, utf8(pattern.c_str()));
   }
   if(f.getAndMode())
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(_gladeXML, "radiobutton_properties_and")), TRUE);
   else
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(_gladeXML, "radiobutton_properties_or")), TRUE);
}

void RGFilterManagerWindow::getSectionFilter(RSectionPackageFilter & f)
{
   //cout <<"RGFilterEditor::getSectionFilter()"<<endl;
   GtkTreeSelection *selection;
   GtkTreeIter iter;
   char *text;
   GList *list;

   GtkWidget *w = glade_xml_get_widget(_gladeXML, "radiobutton_incl");
   assert(w);
   int inclusive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
   f.setInclusive(inclusive == TRUE);
   f.clear();

   selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(_sectionList));
   list = gtk_tree_selection_get_selected_rows(selection, NULL);

   while (list) {
      if (gtk_tree_model_get_iter(GTK_TREE_MODEL(_sectionListStore),
                                  &iter, (GtkTreePath *) list->data)) {
         gtk_tree_model_get(GTK_TREE_MODEL(_sectionListStore), &iter,
                            SECTION_COLUMN, &text, -1);
         f.addSection(string(text));
         g_free(text);
      }
      list = g_list_next(list);
   }
   // free the list
   g_list_foreach(list, (void (*)(void *, void *))gtk_tree_path_free, NULL);
   g_list_free(list);
}

void RGFilterManagerWindow::getStatusFilter(RStatusPackageFilter & f)
{
   //cout <<"RGFilterEditor::getStatusFilter()"<<endl;

   int i;
   int type = 0;
   for (i = 0; i < NrOfStatusBits; i++) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(_statusB[i])))
         type |= StatusMasks[i];
   }
   f.setStatus(type);
}


void RGFilterManagerWindow::getPatternFilter(RPatternPackageFilter &f)
{
   GtkTreeIter iter;
   bool exclude;
   string pattern;
   RPatternPackageFilter::DepType type;
   gchar *dopatt, *what, *text;

   //cout << "RGFilterEditor::getPatternFilter()"<<endl;
   f.reset();

   bool valid =
      gtk_tree_model_get_iter_first(GTK_TREE_MODEL(_patternListStore),
                                    &iter);
   while (valid) {
      /* Walk through the list, reading each row */
      gtk_tree_model_get(GTK_TREE_MODEL(_patternListStore),
                         &iter,
                         PATTERN_DO_COLUMN, &dopatt,
                         PATTERN_WHAT_COLUMN, &what,
                         PATTERN_TEXT_COLUMN, &text, -1);
      // first look at the "act" code
      if (strcmp(dopatt, ActOptions[0]) == 0)
         exclude = false;
      else
         exclude = true;

      // then check the options
      for (int j = 0; DepOptions[j]; j++) {
         if (strcmp(what, _(DepOptions[j])) == 0) {
            type = (RPatternPackageFilter::DepType) j;
            break;
         }
      }
      // then do the pattern (we convert the text to locale to support 
      // translated descriptions)
      f.addPattern(type, utf8_to_locale(text), exclude);

      valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(_patternListStore),
                                       &iter);
      g_free(dopatt);
      g_free(what);
      g_free(text);
   }
   
   f.setAndMode(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(_gladeXML, "radiobutton_properties_and"))));

}

void RGFilterManagerWindow::editFilter()
{
   if (_selectedFilter != NULL)
      editFilter(_selectedFilter);
}

// set the filter setting
void RGFilterManagerWindow::editFilter(RFilter *filter)
{
   //cout << "void RGFilterManagerWindow::editFilter()" << endl;

   setSectionFilter(filter->section);
   setStatusFilter(filter->status);
   setPatternFilter(filter->pattern);
}


void RGFilterManagerWindow::applyChanges(RFilter *filter)
{
   getSectionFilter(filter->section);
   getStatusFilter(filter->status);
   getPatternFilter(filter->pattern);
}


void RGFilterManagerWindow::addFilterAction(GtkWidget *self, void *data)
{
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   RFilter *filter;
   int i = 1;
   gchar *s;

   //cout << "void RGFilterManagerWindow::addFilterAction()" << endl;

   do {
      // no memleak, register filter takes care of it
      filter = new RFilter();
      s = g_strdup_printf(_("New Filter %i"), i);
      filter->setName(s);
      g_free(s);
      i++;
   } while (!me->_filterview->registerFilter(filter));

   GtkTreeIter iter;
   gtk_list_store_append(me->_filterListStore, &iter);
   gtk_list_store_set(me->_filterListStore, &iter,
                      NAME_COLUMN, filter->getName().c_str(),
                      FILTER_COLUMN, filter, -1);
   GtkTreeSelection *selection;
   selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(me->_filterList));
   gtk_tree_selection_select_iter(selection, &iter);
   me->_selectedPath =
      gtk_tree_model_get_path(GTK_TREE_MODEL(me->_filterListStore), &iter);
   me->_selectedFilter = filter;

   me->editFilter();
}



void RGFilterManagerWindow::applyFilterAction(GtkWidget *self, void *data)
{

   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;

   GtkTreeIter iter;
   RFilter *filter;
   //cout << "void RGFilterManagerWindow::applyFilterAction()"<<endl;
   if (me->_selectedPath == NULL) {
      //cout << "_selctedPath == NULL" << endl;
      return;
   }
   if(!gtk_tree_model_get_iter(GTK_TREE_MODEL(me->_filterListStore),
			       &iter, me->_selectedPath)) {
      //cout << "applyFilterAction(): get_iter()==false" << endl;
      return;
   }
   gtk_tree_model_get(GTK_TREE_MODEL(me->_filterListStore), &iter,
                      FILTER_COLUMN, &filter, -1);
   if (filter == NULL) {
      cout << "filter == NULL" << endl;
      return;
   }
   me->applyChanges(filter);
}



void RGFilterManagerWindow::removeFilterAction(GtkWidget *self, void *data)
{
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   GtkTreeIter iter;
   RFilter *filter;

   //cout << "void RGFilterManagerWindow::removeFilterAction()" << endl;

   if (me->_selectedPath == NULL)
      return;

   gtk_tree_model_get_iter(GTK_TREE_MODEL(me->_filterListStore),
                           &iter, me->_selectedPath);
   gtk_tree_model_get(GTK_TREE_MODEL(me->_filterListStore), &iter,
                      FILTER_COLUMN, &filter, -1);
   if (filter) {
      me->_filterview->unregisterFilter(filter);
      delete filter;
      gtk_list_store_remove(me->_filterListStore, &iter);
      me->_selectedPath = NULL;
   }
}



void RGFilterManagerWindow::cancelAction(GtkWidget *self, void *data)
{
   unsigned int i;

   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   //cout << "void RGFilterManagerWindow::cancelAction()"<<endl;

   // unregister all old filters
   guint size = me->_filterview->nrOfFilters();
   for (i = 0; i < size; i++) {
      RFilter *filter = me->_filterview->findFilter(0);
      me->_filterview->unregisterFilter(filter);
      delete filter;
   }

   // restore the old filters
   for (i = 0; i < me->_saveFilters.size(); i++)
      me->_filterview->registerFilter(me->_saveFilters[i]);

   me->close();
}

void RGFilterManagerWindow::okAction(GtkWidget *self, void *data)
{
   RGFilterManagerWindow *me = (RGFilterManagerWindow *) data;
   //cout << "void RGFilterManagerWindow::okAction()"<<endl;

   me->applyFilterAction(self, data);
   me->_filterview->storeFilters();
   me->close();
}






