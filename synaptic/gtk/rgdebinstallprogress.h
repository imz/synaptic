/* rgdebinstallprogress.h
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


#ifndef _RGDEBINSTALLPROGRESS_H_
#define _RGDEBINSTALLPROGRESS_H_


#include "rinstallprogress.h"
#include "rggladewindow.h"
#include<map>

class RGMainWindow;


class RGDebInstallProgress:public RInstallProgress, public RGGladeWindow 
{
   // the various stages of dpkg
   static const int NR_REMOVE_STAGES=3;
   static char* remove_stages[NR_REMOVE_STAGES];

   static const int NR_PURGE_STAGES=4;
   static char *purge_stages[NR_PURGE_STAGES];

   // purge a already removed pkg
   static const int NR_PURGE_ONLY_STAGES=2;
   static char *purge_only_stages[NR_PURGE_ONLY_STAGES]; 

   static const int NR_INSTALL_STAGES=4;
   static char *install_stages[NR_INSTALL_STAGES];

   static const int NR_UPDATE_STAGES=5;
   static char *update_stages[NR_UPDATE_STAGES];

   static const int NR_REINSTALL_STAGES=6;
   static char *reinstall_stages[NR_REINSTALL_STAGES];


   // widgets
   GtkWidget *_label_status;
   GtkWidget *_labelSummary;

   GtkWidget *_pbar;
   GtkWidget *_pbarTotal;

   bool _startCounting;

   int _progress;
   int _totalActions;

   // this map contains the name and a pointer to the stages arrays
   map<string, char**> _actionsMap;

   // this map contains what stage is already completted
   map<string, int> _stagesMap;

   // readable states 
   map<string,string> _transDpkgStates;

 protected:
   virtual void startUpdate();
   virtual void updateInterface();
   virtual void finishUpdate();

   virtual void prepare(RPackageLister *lister);
   
   void conffile(gchar *conffile, gchar *status);

 public:
   RGDebInstallProgress(RGMainWindow *main, RPackageLister *lister);

};

#endif
