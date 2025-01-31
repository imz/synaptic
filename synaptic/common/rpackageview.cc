/* rpackageview.cc - Package sectioning system
 * 
 * Copyright (c) 2000, 2001 Conectiva S/A 
 *               2002 Michael Vogt <mvo@debian.org>
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
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/configuration.h>
#include <rpackage.h>
#include <rpackageview.h>
#include <rconfiguration.h>

#include <map>
#include <vector>
#include <sstream>

#include "sections_trans.h"

using namespace std;

bool RPackageView::setSelected(const string &name)
{
   map<string, vector<RPackage *> >::iterator I = _view.find(name);
   if (I != _view.end()) {
      _hasSelection = true;
      _selectedName = name;
      _selectedView = (*I).second;
   } else {
      clearSelection();
   }
   return _hasSelection;
}

vector<string> RPackageView::getSubViews()
{
   vector<string> subViews;
   for (map<string, vector<RPackage *> >::iterator I = _view.begin();
        I != _view.end(); I++)
      subViews.push_back((*I).first);
   return subViews;
}

void RPackageView::clear()
{
   clearSelection();
   _view.clear();
}

void RPackageView::clearSelection()
{
   _hasSelection = false;
   _selectedName.clear();
   _selectedView.clear();
}

void RPackageView::refresh()
{
   if(_config->FindB("Debug::Synaptic::View",false))
      ioprintf(clog, "RPackageView::refresh(): '%s'\n",
	       getName().c_str());

   _view.clear();
   for(unsigned int i=0;i<_all.size();i++) {
      if(_all[i])
	 addPackage(_all[i]);
   }
}

void RPackageViewSections::addPackage(RPackage *package)
{
   string str = trans_section(package->section());
   _view[str].push_back(package);
};

RPackageViewStatus::RPackageViewStatus(vector<RPackage *> &allPkgs) 
   : RPackageView(allPkgs), markUnsupported(false)
{
   if(_config->FindB("Synaptic::mark-unsupported",false)) {
      markUnsupported = true;
      string components = _config->Find("Synaptic::supported-components", "main updates/main");

      stringstream sstream(components);
      string s;
      while(!sstream.eof()) {
	 sstream >> s;
	 supportedComponents.push_back(s);
      }
   }
};

void RPackageViewStatus::addPackage(RPackage *pkg)
{
   string str;
   int flags = pkg->getFlags();
   string component = pkg->component();
   bool unsupported = false;
   
   // we mark packages as unsupported if requested
   if(markUnsupported) {
      unsupported = true;
      for(unsigned int i=0;i<supportedComponents.size();i++) {
	 if(supportedComponents[i] == component) {
	    unsupported = false;
	    break;
	 }
      }
   }

   if(flags & RPackage::FInstalled) {
      if( !(flags & RPackage::FNotInstallable) && unsupported)
	 str = _("Installed (unsupported)");
      else 
	 str = _("Installed");
   } else {
      if( unsupported )
	 str = _("Not installed (unsupported)");
      else
	 str = _("Not installed");
   }
   _view[str].push_back(pkg);

   str.clear();
   if (flags & RPackage::FNowBroken)
      str = _("Broken dependencies");
   else if (flags & RPackage::FNew)
      str = _("New in repository");
   else if (flags & RPackage::FPinned) 
      str = _("Pinned");
   else if ((flags & RPackage::FNotInstallable) &&
	    !(flags & RPackage::FResidualConfig) &&
	    (flags & RPackage::FInstalled))
      str = _("Installed (local or obsolete)");
   else if (flags & RPackage::FInstalled) {
      if (flags & RPackage::FOutdated)
	 str = _("Installed (upgradable)");
   } else {
      if (flags & RPackage::FResidualConfig)
	 str = _("Not installed (residual config)");
   }

   if(!str.empty())
      _view[str].push_back(pkg);
}


//------------------------------------------------------------------

void RPackageViewSearch::addPackage(RPackage *pkg)
{
   string str;
   const char *tmp=NULL;
   bool global_found=true;

   if(!pkg || searchStrings.empty())
      return;

   // build the string
   switch(searchType) {
   case RPatternPackageFilter::Name:
      tmp = pkg->name();
      break;
   case RPatternPackageFilter::Version:
      tmp = pkg->availableVersion();
      break;
   case RPatternPackageFilter::Description:
      str = pkg->name();
      str += string(pkg->summary());
      str += string(pkg->description());
      break;
   case RPatternPackageFilter::Maintainer:
      tmp = pkg->maintainer();
      break;
   case RPatternPackageFilter::Depends: 
      {
	 vector<DepInformation> d = pkg->enumDeps(true);
	 for(unsigned int i=0;i<d.size();i++)
	    str += string(d[i].name);
	 break; 
      }
   case RPatternPackageFilter::Provides: 
      {
	 vector<string> d = pkg->provides();
	 for(unsigned int i=0;i<d.size();i++)
	    str += d[i];
	 break;
      }
   }

   if(tmp!=NULL)
      str = tmp;
      
   // find the search pattern in the string "str"
   for(unsigned int i=0;i<searchStrings.size();i++) {
      string searchString = searchStrings[i];

      if(!str.empty() && strcasestr(str.c_str(), searchString.c_str())) {
	 global_found &= true;
      } else {
	 global_found &= false;
      }
   }
   if(global_found) {
      _view[searchName].push_back(pkg);
      found++;
   }

   // FIXME: we push a _lot_ of empty pkgs here :(
   // push a empty package in the view to make sure that the view is actually
   // displayed
   //_view[searchString].push_back(NULL);
}

int RPackageViewSearch::setSearch(const string &aSearchName, int type, 
				  const string &searchString)
{
   found = 0;
   searchType = type;
   searchName = aSearchName;

   _view[searchName].clear();
   searchStrings.clear();

   // tokenize the str and add to the searchString vector
   stringstream sstream(searchString);
   string s;
   while(!sstream.eof()) {
      sstream >> s;
      searchStrings.push_back(s);
   }

   // reapply search when a new search strng is giben
   for(unsigned int i=0;i<_all.size();i++) 
      if(_all[i]) 
	 addPackage(_all[i]);

   return found;
}

//------------------------------------------------------------------

RPackageViewFilter::RPackageViewFilter(vector<RPackage *> &allPkgs) 
   : RPackageView(allPkgs)
{
   // restore the filters
   restoreFilters();

   refreshFilters();
}

void RPackageViewFilter::refreshFilters()
{
   _view.clear();

   // create a empty sub-views for each filter
   for (vector<RFilter *>::iterator I = _filterL.begin();
	I != _filterL.end(); I++) {
      _view[(*I)->getName()].push_back(NULL);
   }
}

int RPackageViewFilter::getFilterIndex(RFilter *filter)
{
  if (filter == NULL)
     filter = findFilter(_selectedName);
  for(unsigned int i=0;i<_filterL.size();i++)  {
    if(filter == _filterL[i])
      return i;
  }
  return -1;
}


RPackageViewFilter::iterator RPackageViewFilter::begin() 
{ 
//    cout << "RPackageViewFilter::begin() " << _selectedName <<  endl;

   string name = _selectedName;
   RFilter *filter = findFilter(name);

   if(filter != NULL) {
      _view[name].clear();

      for(unsigned int i=0;i<_all.size();i++) {
	 if(_all[i] && filter->apply(_all[i]))
	    _view[name].push_back(_all[i]);
      }
      _selectedView = _view[name];
   }

   return _selectedView.begin(); 
}

void RPackageViewFilter::refresh()
{
   //cout << "RPackageViewFilter::refresh() " << endl;

   refreshFilters();
}


vector<string> RPackageViewFilter::getFilterNames()
{
   vector<string> filters;
   for (unsigned int i = 0; i != _filterL.size(); i++)
      filters.push_back(_filterL[i]->getName());
   return filters;
}


void RPackageViewFilter::addPackage(RPackage *pkg)
{
   // nothing to do for now, may add some sort of caching later
}

const set<string>& RPackageViewFilter::getSections() 
{ 
   if(_sectionList.empty())
      for(unsigned int i=0;i<_all.size();i++)
	 if(_all[i])
	    _sectionList.insert(_all[i]->section());
   return _sectionList; 
};


void RPackageViewFilter::storeFilters()
{
   ofstream out;

   if (!RFilterDataOutFile(out))
      return;

   for (vector<RFilter *>::const_iterator iter = _filterL.begin();
        iter != _filterL.end(); iter++) {

      (*iter)->write(out);
   }

   out.close();
}

void RPackageViewFilter::restoreFilters()
{
   Configuration config;
   RReadFilterData(config);

   RFilter *filter;
   const Configuration::Item *top = config.Tree("filter");
   for (top = (top == 0 ? 0 : top->Child); top != 0; top = top->Next) {
      filter = new RFilter();
      filter->setName(top->Tag);

      string filterkey = "filter::" + top->Tag;
      if (filter->read(config, filterkey)) {
         registerFilter(filter);
      } else {
         delete filter;
      }
   }

   // Introduce new preset filters in the current config file.
   // Already existent filters will be ignored, since the name
   // will clash.
   makePresetFilters();
}

bool RPackageViewFilter::registerFilter(RFilter *filter)
{
   string Name = filter->getName();
   for (vector<RFilter *>::const_iterator I = _filterL.begin();
        I != _filterL.end(); I++) {
      if ((*I)->getName() == Name) {
         delete filter;
         return false;
      }
   }
   _filterL.push_back(filter);
   return true;
}

void RPackageViewFilter::unregisterFilter(RFilter *filter)
{
   for (vector<RFilter *>::iterator I = _filterL.begin();
        I != _filterL.end(); I++) {
      if (*I == filter) {
         _filterL.erase(I);
         return;
      }
   }
}

RFilter* RPackageViewFilter::findFilter(const string &name)
{
   RFilter *filter=NULL;
   // find filter
   for (vector<RFilter *>::iterator I = _filterL.begin();
	I != _filterL.end(); I++) {
      if((*I)->getName() == name) {
	 filter = (*I);
      }
   }
   return filter;
}

// we make only preset filters that are not covered by the status view
void RPackageViewFilter::makePresetFilters()
{
   RFilter *filter;

   // Notice that there's a little hack in filter names below. They're
   // saved *without* i18n, but there's an i18n version for i18n. This
   // allows i18n to be done in RFilter.getName().
   {
      filter = new RFilter();
      filter->preset = true;
      filter->setName("Search Filter");
      _("Search Filter");
      registerFilter(filter);
   }
#ifdef HAVE_RPM
   {
      filter = new RFilter();
      filter->pattern.addPattern(RPatternPackageFilter::Name,
                                 "^task-.*", false);
      filter->setName("Tasks"); _("Tasks");
      registerFilter(filter);
   }
   {
      filter = new RFilter();
      filter->reducedview.enable();
      filter->setName("Reduced View"); _("Reduced View");
      registerFilter(filter);
   }
#endif
   {
      filter = new RFilter();
      filter->preset = true;
      filter->status.setStatus(RStatusPackageFilter::Broken);
      filter->setName("Broken"); _("Broken");
      registerFilter(filter);
   }
   {
      filter = new RFilter();
      filter->preset = true;
      filter->status.setStatus(RStatusPackageFilter::MarkInstall
                               | RStatusPackageFilter::MarkRemove
                               | RStatusPackageFilter::Broken);
      filter->setName("Marked Changes"); _("Marked Changes");
      registerFilter(filter);
   }
#ifndef HAVE_RPM
   {
      filter = new RFilter();
      filter->preset = true;
      filter->pattern.addPattern(RPatternPackageFilter::Depends,
                                 "^debconf", false);
      // TRANSLATORS: This is a filter that will give you all packages
      // with debconf support (that can be reconfigured with debconf)
      filter->setName("Package with Debconf"); _("Package with Debconf");
      registerFilter(filter);
   }
#endif
   filter = new RFilter();
   filter->preset = true;
   filter->status.setStatus(RStatusPackageFilter::UpstreamUpgradable);
   filter->setName("Upgradable (upstream)"); _("Upgradable (upstream)");
   registerFilter(filter);
}

void RPackageViewOrigin::addPackage(RPackage *package)
{
   string component =  package->component();
   string origin = package->getCanidateOrigin();
   if(origin == "")
      origin = _("Local");
   if(component == "")
      component = _("Unknown");
   _view[origin+"/"+component].push_back(package);
 };

// vim:sts=3:sw=3
