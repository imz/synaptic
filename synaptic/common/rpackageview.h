/* rpackageview.h - Package sectioning system
 * 
 * Copyright (c) 2004 Conectiva S/A 
 *               2004 Michael Vogt <mvo@debian.org>
 * 
 * Author: Gustavo Niemeyer
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


#ifndef RPACKAGEVIEW_H
#define RPACKAGEVIEW_H

#include <string>
#include <map>

#include "rpackage.h"
#include "rpackagefilter.h"

#include "i18n.h"

using namespace std;

struct RFilter;

enum {PACKAGE_VIEW_SECTION,
      PACKAGE_VIEW_STATUS,
      PACKAGE_VIEW_ORIGIN,
      PACKAGE_VIEW_CUSTOM,
      PACKAGE_VIEW_SEARCH,
      N_PACKAGE_VIEWS
};

class RPackageView {
 protected:

   map<string, vector<RPackage *> > _view;

   bool _hasSelection;
   string _selectedName;

   // packages in selected
   vector<RPackage *> _selectedView;

   // all packages in current global filter
   vector<RPackage *> &_all;

 public:
   RPackageView(vector<RPackage *> &allPackages): _all(allPackages) {};
   virtual ~RPackageView() {};

   bool hasSelection() { return _hasSelection; };
   string getSelected() { return _selectedName; };
   bool setSelected(const string &name);
   void showAll() { 
      _selectedView = _all; 
      _hasSelection = false;
      _selectedName.clear();
   };

   vector<string> getSubViews();

   virtual string getName() = 0;
   virtual void addPackage(RPackage *package) = 0;

   typedef vector<RPackage *>::iterator iterator;

   virtual iterator begin() { return _selectedView.begin(); };
   virtual iterator end() { return _selectedView.end(); };

   virtual void clear();
   virtual void clearSelection();

   virtual void refresh();
};



class RPackageViewSections : public RPackageView {
 public:
   RPackageViewSections(vector<RPackage *> &allPkgs) : RPackageView(allPkgs) {};

   virtual string getName() override {
      return _("Sections");
   }

   virtual void addPackage(RPackage *package) override;
};

class RPackageViewAlphabetic : public RPackageView {
 public:
   RPackageViewAlphabetic(vector<RPackage *> &allPkgs) : RPackageView(allPkgs) {};
   virtual string getName() override {
      return _("Alphabetic");
   }

   virtual void addPackage(RPackage *package) override {
      char letter[2] = { ' ', '\0' };
      letter[0] = toupper(package->name()[0]);
      _view[letter].push_back(package);
   }
};

class RPackageViewOrigin : public RPackageView {
 public:
   RPackageViewOrigin(vector<RPackage *> &allPkgs) : RPackageView(allPkgs) {};
   virtual string getName() override {
      return _("Origin");
   }

   virtual void addPackage(RPackage *package) override;
};

class RPackageViewStatus:public RPackageView {
 protected:
   // mark the software as unsupported in status view
   bool markUnsupported;
   vector<string> supportedComponents;

 public:
   RPackageViewStatus(vector<RPackage *> &allPkgs);

   virtual string getName() override {
      return _("Status");
   }

   virtual void addPackage(RPackage *package) override;
};

class RPackageViewSearch : public RPackageView {
 protected:
   vector<string> searchStrings;
   string searchName;
   int searchType;
   int found; // nr of found pkgs for the last search
 public:
   RPackageViewSearch(vector<RPackage *> &allPkgs) 
      : RPackageView(allPkgs), found(0) {};

   int setSearch(const string &searchName, int type, const string &searchString);

   virtual string getName() override {
      return _("Search History");
   }

   virtual void addPackage(RPackage *package) override;

   // no-op
   virtual void refresh() override {}
};


class RPackageViewFilter : public RPackageView {
 protected:
   vector<RFilter *> _filterL;
   set<string> _sectionList;   // list of all available package sections

 public:
   void storeFilters();
   void restoreFilters();
   // called after the filtereditor was run
   void refreshFilters();
   virtual void refresh() override;

   bool registerFilter(RFilter *filter);
   void unregisterFilter(RFilter *filter);

   void makePresetFilters();

   RFilter* findFilter(const string &name);
   unsigned int nrOfFilters() { return _filterL.size(); };
   RFilter *findFilter(unsigned int index) {
      if (index > _filterL.size())
         return NULL;
      else
         return _filterL[index];
   };

   // used by kynaptic
   int getFilterIndex(RFilter *filter);

   vector<string> getFilterNames();
   const set<string> &getSections();

   RPackageViewFilter(vector<RPackage *> &allPkgs);

   // build packages list on "demand"
   virtual iterator begin() override;

   // we never need to clear because we build the view "on-demand"
   virtual void clear() override {clearSelection();}

   virtual string getName() override {
      return _("Custom");
   }

   virtual void addPackage(RPackage *package) override;
};



#endif

// vim:sts=3:sw=3
