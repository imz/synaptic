/* rgrepositorywin.h - gtk editor for the sources.list file
 * 
 * Copyright (c) (c) 1999 Patrick Cole <z@amused.net>
 *               (c) 2002 Synaptic development team 
 *
 * Author: Patrick Cole <z@amused.net>
 *         Michael Vogt <mvo@debian.org>
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


#ifndef _RGREPOSITORYWIN_H
#define _RGREPOSITORYWIN_H

#include<gtk/gtk.h>
#include "rsources.h"
#include "rggladewindow.h"
#include "rgvendorswindow.h"
#include "rguserdialog.h"

typedef list<SourcesList::SourceRecord*>::iterator SourcesListIter;
typedef list<SourcesList::VendorRecord*>::iterator VendorsListIter;

class RGRepositoryEditor : RGGladeWindow {
    SourcesList _lst,_savedList;

    int _selectedRow;

    // the gtktreeview
    GtkWidget *_sourcesListView;
    GtkListStore *_sourcesListStore;
    GtkTreeIter *_lastIter;

    GtkWidget *_editTable;
    GtkWidget *_optVendor;
    GtkWidget *_optVendorMenu;
    GtkWidget *_entryURI;
    GtkWidget *_entrySect;
    GtkWidget *_optType;
    GtkWidget *_optTypeMenu;
    GtkWidget *_entryDist;
    //GtkWidget *_cbEnabled;

    RGUserDialog *_userDialog;

    bool _applied;
    GdkColor _gray;

    void UpdateVendorMenu();
    int VendorMenuIndex(string VendorID);

    // static event handlers
    static void DoClear(GtkWidget *, gpointer);
    static void DoAdd(GtkWidget *, gpointer);
    static void DoRemove(GtkWidget *, gpointer);
    static void DoOK(GtkWidget *, gpointer);
    static void DoCancel(GtkWidget *, gpointer);
    static void VendorsWindow(GtkWidget *, gpointer);
    static void SelectionChanged(GtkTreeSelection *selection, gpointer data);

    // get values
    void doEdit();


 public:
    RGRepositoryEditor(RGWindow *parent);
    ~RGRepositoryEditor();
    
    bool Run();
};

#endif
