/* rpackagelister.cc - package cache and list manipulation
 * 
 * Copyright (c) 2000-2003 Conectiva S/A 
 *               2002 Michael Vogt <mvo@debian.org>
 * 
 * Author: Alfredo K. Kojima <kojima@conectiva.com.br>
 *         Michael Vogt <mvo@debian.org>
 * 
 * Portions Taken from apt-get
 *   Copyright (C) Jason Gunthorpe
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
#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <sstream>

#include "i18n.h"

#include "rpackagelister.h"
#include "rpackagecache.h"
#include "rpackagefilter.h"
#include "rconfiguration.h"
#include "raptoptions.h"
#include "rinstallprogress.h"
#include "rcacheactor.h"

#include <apt-pkg/error.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/clean.h>

#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/strutl.h>

#include <algorithm>
#include <cstdio>

#ifndef HAVE_RPM
#include "sections_trans.h"
#endif

using namespace std;

RPackageLister::RPackageLister()
    :  _records(0), _packages(0), _packageindex(0), _filter(0)
{
    _cache = new RPackageCache();
    
    _searchData.pattern = NULL;
    _searchData.isRegex = false;
    _displayMode = (treeDisplayMode)_config->FindI("Synaptic::TreeDisplayMode",0);
    _updating = true;
    _sortMode = TREE_SORT_NAME;
    
    memset(&_searchData, 0, sizeof(_searchData));

#ifdef HAVE_RPM
    string Recommends = _config->Find("Synaptic::RecommendsFile",
				      "/etc/apt/metadata");
    if (FileExists(Recommends))
	_actors.push_back(new RCacheActorRecommends(this, Recommends));
#endif
    _actors.push_back(new RCacheActorPkgTrack(this));
}

RPackageLister::~RPackageLister()
{
    for (vector<RCacheActor*>::iterator I = _actors.begin();
	 I != _actors.end(); I++)
	delete (*I);

    delete _cache;
}

static string getServerErrorMessage(string errm)
{
   string msg;
   unsigned int pos = errm.find("server said");
   if (pos != string::npos) {
      msg = string(errm.c_str()+pos+sizeof("server said"));
      if (msg[0] == ' ')
	  msg = msg.c_str()+1;
   }
   return msg;
}

void RPackageLister::notifyPreChange(RPackage *pkg)
{
    for (vector<RPackageObserver*>::const_iterator I = _packageObservers.begin();
	 I != _packageObservers.end(); I++) {
	(*I)->notifyPreFilteredChange();
    }
}

void RPackageLister::notifyPostChange(RPackage *pkg)
{
    reapplyFilter();

    for (vector<RPackageObserver*>::const_iterator I = _packageObservers.begin();
	 I != _packageObservers.end(); I++) {
	(*I)->notifyPostFilteredChange();
    }

    if (pkg != NULL) {
	for (vector<RPackageObserver*>::const_iterator I = _packageObservers.begin();
	     I != _packageObservers.end(); I++) {
	    (*I)->notifyChange(pkg);
	}
    }
}

void RPackageLister::notifyChange(RPackage *pkg)
{
//     cout << "RPackageLister::notifyChange(RPackage *pkg): " 
// 	 << _packageObservers.size() 
// 	 << endl;

    notifyPreChange(pkg);
    notifyPostChange(pkg);
}

void RPackageLister::unregisterObserver(RPackageObserver *observer)
{
//     cout << "RPackageLister::unregisterObserver(RPackageObserver *observer)" << endl;

    vector<RPackageObserver*>::iterator I;
    I = find(_packageObservers.begin(), _packageObservers.end(), observer);
    if(I != _packageObservers.end())
	_packageObservers.erase(I);
#if 0
    else 
	cout << "unregisterObserver() failed" << endl;
#endif
}

void RPackageLister::registerObserver(RPackageObserver *observer)
{
    _packageObservers.push_back(observer);
}


void RPackageLister::notifyCacheOpen()
{
    undoStack.clear();
    redoStack.clear();
    for (vector<RCacheObserver*>::const_iterator I = _cacheObservers.begin();
	 I != _cacheObservers.end(); I++) {
	(*I)->notifyCacheOpen();
    }
    
}

void RPackageLister::notifyCachePreChange()
{
    for (vector<RCacheObserver*>::const_iterator I = _cacheObservers.begin();
	 I != _cacheObservers.end(); I++) {
	(*I)->notifyCachePreChange();
    }
}

void RPackageLister::notifyCachePostChange()
{
    for (vector<RCacheObserver*>::const_iterator I = _cacheObservers.begin();
	 I != _cacheObservers.end(); I++) {
	(*I)->notifyCachePostChange();
    }
}

void RPackageLister::unregisterCacheObserver(RCacheObserver *observer)
{
    //cout << "RPackageLister::unregisterCacheObserver(RCacheObserver *observer)"<< endl;
    //remove(_cacheObservers.begin(), _cacheObservers.end(), observer);
    vector<RCacheObserver*>::iterator I;
    I = find(_cacheObservers.begin(), _cacheObservers.end(), observer);
    if(I != _cacheObservers.end())
	_cacheObservers.erase(I);
#if 0
    else
	cout << "unregisterCacheObserver() failed" << endl;
#endif
}

void RPackageLister::registerCacheObserver(RCacheObserver *observer)
{
    _cacheObservers.push_back(observer);
}


void RPackageLister::makePresetFilters()
{
    RFilter *filter;

    // Notice that there's a little hack in filter names below. They're
    // saved *without* i18n, but there's an i18n version for i18n. This
    // allows i18n to be done in RFilter.getName().
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->setName("Search Filter"); _("Search Filter");
	registerFilter(filter);
    }
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->status.setStatus((int)RStatusPackageFilter::Installed);
	filter->setName("Installed"); _("Installed");
	registerFilter(filter);
    }
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->status.setStatus((int)RStatusPackageFilter::NotInstalled);
	filter->setName("Not Installed"); _("Not Installed");
	registerFilter(filter);
    }
#ifdef HAVE_RPM
    {
	filter = new RFilter(this);
	filter->pattern.addPattern(RPatternPackageFilter::Name,
				  "^task-.*", false);
	filter->setName("Tasks"); _("Tasks");
	registerFilter(filter);
    }
    {
	filter = new RFilter(this);
	filter->reducedview.enable();
	filter->setName("Reduced View"); _("Reduced View");
	registerFilter(filter);
    }
#endif
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->status.setStatus((int)RStatusPackageFilter::Upgradable);
	filter->setName("Upgradable"); _("Upgradable");
	registerFilter(filter);
    }
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->status.setStatus(RStatusPackageFilter::Broken);
	filter->setName("Broken"); _("Broken");
	registerFilter(filter);
    }
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->status.setStatus(RStatusPackageFilter::MarkInstall
				 |RStatusPackageFilter::MarkRemove
				 |RStatusPackageFilter::Broken);
	filter->setName("Queued Changes"); _("Queued Changes");
	registerFilter(filter);
    }
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->status.setStatus(RStatusPackageFilter::NewPackage);
	filter->setName("New in archive"); _("New in archive");
	registerFilter(filter);
    }
#ifndef HAVE_RPM
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->status.setStatus(RStatusPackageFilter::ResidualConfig);
	filter->setName("Residual config"); _("Residual config");
	registerFilter(filter);
    }
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->pattern.addPattern(RPatternPackageFilter::Depends,
				  "^debconf", false);
	filter->setName("Pkg with Debconf"); _("Pkg with Debconf");
	registerFilter(filter);
    }
    {
	filter = new RFilter(this);
	filter->preset = true;
	filter->status.setStatus(RStatusPackageFilter::NotInstallable);
	filter->setName("Obsolete or locally installed"); _("Obsolete or locally installed");
	registerFilter(filter);
    }
#endif
}


void RPackageLister::restoreFilters()
{
  Configuration config;
  RReadFilterData(config);

  RFilter *filter;
  const Configuration::Item *top = config.Tree("filter");
  for (top = (top == 0?0:top->Child); top != 0; top = top->Next) {
    filter = new RFilter(this);
    filter->setName(top->Tag);
    
    string filterkey = "filter::"+top->Tag;
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


void RPackageLister::storeFilters()
{
    ofstream out;
    
    if (!RFilterDataOutFile(out))
	return;

    for (vector<RFilter*>::const_iterator iter = _filterL.begin();
	 iter != _filterL.end(); iter++) {

	(*iter)->write(out);
    }
    
    out.close();
}


bool RPackageLister::registerFilter(RFilter *filter)
{
    string Name = filter->getName();
    for (vector<RFilter*>::const_iterator I = _filterL.begin();
	 I != _filterL.end(); I++) {
	if ((*I)->getName() == Name) {
	    delete filter;
	    return false;
	}
    }
    _filterL.push_back(filter);
    return true;
}


void RPackageLister::unregisterFilter(RFilter *filter)
{
    for (vector<RFilter*>::iterator I = _filterL.begin();
	 I != _filterL.end(); I++) {
	if (*I == filter) {
	    _filterL.erase(I);
	    return;
	}
    }
}


bool RPackageLister::check()
{   
    if (_cache->deps() == NULL)
	return false;
    // Nothing is broken
    if (_cache->deps()->BrokenCount() != 0) {
	return false;
    }
    
    return true;
}

bool RPackageLister::upgradable()
{
    return _cache!=NULL && _cache->deps()!=NULL;
}



// We have to reread the cache if we're using "pin". 
bool RPackageLister::openCache(bool reset)
{
    static bool firstRun=true;
    //cout << "RPackageLister::openCache(bool reset)" << endl;

    // flush old errors
    _error->Discard();

    if (reset) {
	if (!_cache->reset(*_progMeter)) {
	    _progMeter->Done();
	    return false;
	}
    } else {
	if (!_cache->open(*_progMeter)) {
	    _progMeter->Done();
	    return false;
        }
    }
    _progMeter->Done();

    pkgDepCache *deps = _cache->deps();

    // Apply corrections for half-installed packages
    if (pkgApplyStatus(*deps) == false)
	return 	_error->Error(_("Internal error recalculating dependency cache. Please report."));
    
    if (_error->PendingError()) {
	return 	_error->Error(_("Internal error recalculating dependency cache. Please report."));
    }

    //cout << "    _treeOrganizer.clear();" << endl;
    _treeOrganizer.clear();

#ifdef HAVE_RPM
    if (_records) {
      // mvo: BUG: this will sometimes segfault. maybe bug in apt?
      //      segfault can be triggered by changing the repositories
      delete _records;
    }
#endif

    _records = new pkgRecords(*deps);
    if (_error->PendingError()) {
	return 	_error->Error(_("Internal error recalculating dependency cache. Please report."));
    }
    
    if (_packages) {
	for(int i=0;_packages[i] != NULL;i++)
	    delete _packages[i];
	delete [] _packages;
	delete [] _packageindex;
    }

    int PackageCount = deps->Head().PackageCount;
    _packages = new RPackage *[PackageCount];
    memset(_packages, 0, sizeof(*_packages)*PackageCount);

    _packageindex = new int[PackageCount];
    fill_n(_packageindex, PackageCount, -1);

    pkgCache::PkgIterator I = deps->PkgBegin();

    string pkgName;
    _count = 0;
    _installedCount = 0;
    map<string,RPackage*> pkgmap;
    set<string> sectionSet;

    for (; I.end() != true; I++) {

	if (I->CurrentVer != 0)
	    _installedCount++;
	else if (I->VersionList==0) 
	    // exclude virtual packages
	    continue;

	RPackage *pkg = new RPackage(this, deps, _records, I);
	_packageindex[I->ID] = _count;
	_packages[_count++] = pkg;
	
	pkgName = pkg->name();
	
	pkgmap[pkgName] = pkg;

	// find out about new packages
	if(firstRun) {
	    allPackages.insert(pkgName);
	} else if(allPackages.find(pkgName) == allPackages.end()) {
	    pkg->setNew();
	    _roptions->setPackageNew(pkgName.c_str());
	    allPackages.insert(pkgName);
	}

	// gather list of sections
	if (I.Section()) {
	    sectionSet.insert(I.Section());
	} else {
	    cerr << "Package " << I.Name() << " has no section?!" << endl;
	}
#if 0
	// gather tree of virtual packages (using the provides field)
	// format:
	//  c-compiler
	//     - gcc-2.95
	//     - gcc-3.2 
	static map<string,tree<string>::iterator> itermap;

	if(pkg->provides().size() > 1) {
	    string name(pkg->name());
	    //cout << "pkg: " << name << " provides: " << endl;
	    vector<const char *> p=pkg->provides();
	    tree<string>::iterator it;
	    for(int i=0;i<p.size();i++) {
		string provides(p[i]);

		if(itermap.find(provides) == itermap.end()) {
		    it = virtualPackages.insert(virtualPackages.begin(), provides);
		    itermap[provides] = it;
		}
		else
		    it = itermap[provides];
		virtualPackages.append_child(it, name);
	    }
	}
#endif
    }

#if 0
    // output the virtualPackages Tree
    tree<string>::iterator it = virtualPackages.begin();
    while(it != virtualPackages.end()) {
	if(virtualPackages.depth(it) > 0)
	    cout << "\t";
	cout << (*it) << endl;
	++it;
    }
#endif

    _sectionList.resize(sectionSet.size());
    copy(sectionSet.begin(), sectionSet.end(), _sectionList.begin());


    for (I = deps->PkgBegin(); I.end() == false; I++) {
	// this is a virtual pkg
	if (I->VersionList == 0) {
	    // find the owner of this virtual package and attach it there
 	    if (I->ProvidesList == 0) 
 		continue;
	    string name = I.ProvidesList().OwnerPkg().Name();
	    //cout << I.Name() << " provides: " << name << endl;
	    map<string,RPackage*>::iterator I2 = pkgmap.find(name);
	    if (I2 != pkgmap.end()) {
		(*I2).second->addVirtualPackage(I);
		//cout << (*I2).first << " provides: " << name << endl;
	    }
	}
    }

    applyInitialSelection();

    _updating = false;
    if (reset) {
	reapplyFilter();
    } else {   
	// set default filter (no filter)
	setFilter();
    }

    // mvo: put it here for now
    notifyCacheOpen();

    firstRun=false;

    return true;
}



void RPackageLister::applyInitialSelection()
{
  //cout << "RPackageLister::applyInitialSelection()" << endl;

  _roptions->rereadOrphaned();
  _roptions->rereadDebconf();
  
  for (unsigned i = 0; i < _count; i++) {
    if (_roptions->getPackageLock(_packages[i]->name())) {
      _packages[i]->setPinned(true);
    }

    if (_roptions->getPackageNew(_packages[i]->name())) {
      _packages[i]->setNew(true);
    }

    if (_roptions->getPackageOrphaned(_packages[i]->name())) {
      //cout << "orphaned: " << _packages[i]->name() << endl;
      _packages[i]->setOrphaned(true);
    }
  }
}



RPackage *RPackageLister::getElement(pkgCache::PkgIterator &iter)
{
    int index = _packageindex[iter->ID];
    if (index != -1)
	return _packages[index];
    return NULL;
}

RPackage *RPackageLister::getElement(string Name)
{
    pkgCache::PkgIterator Pkg = _cache->deps()->FindPkg(Name);
    if (Pkg.end() == false)
	return getElement(Pkg);
    return NULL;
}


int RPackageLister::getElementIndex(RPackage *pkg)
{
    for (unsigned int i = 0; i < _displayList.size(); i++) {
	if (_displayList[i] == pkg)
	    return i;
    }
    return -1;
}

bool RPackageLister::fixBroken()
{
    if (_cache->deps() == NULL)
	return false;

    if (_cache->deps()->BrokenCount() == 0)
	return true;
    
    pkgProblemResolver Fix(_cache->deps());
    
    Fix.InstallProtect();
    if (Fix.Resolve(true) == false)
	return false;
    
    reapplyFilter();

    return true;
}


bool RPackageLister::upgrade()
{
    if (pkgAllUpgrade(*_cache->deps()) == false) {
	return _error->Error(_("Internal Error, AllUpgrade broke stuff. Please report."));
    }
    
    //reapplyFilter();
    notifyChange(NULL);
    
    return true;
}


bool RPackageLister::distUpgrade()
{
    if (pkgDistUpgrade(*_cache->deps()) == false)
    {
	cout << _("dist upgrade Failed") << endl;
	return false;
    }
    
    //reapplyFilter();
    notifyChange(NULL);    

    return true;
}



void RPackageLister::setFilter(int index)
{
    if (index < 0 || index >= _filterL.size() ) {
	_filter = NULL;
    } else {
	_filter = findFilter(index);
    }

    reapplyFilter();
}

void RPackageLister::setFilter(RFilter *filter)
{
  _filter = NULL;
  for(unsigned int i=0;i<_filterL.size();i++)  {
    if(filter == _filterL[i]) {
      _filter = _filterL[i];
      break;
    }
  }

  reapplyFilter();
}

int RPackageLister::getFilterIndex(RFilter *filter)
{
  if (filter == NULL)
    filter = _filter;
  for(unsigned int i=0;i<_filterL.size();i++)  {
    if(filter == _filterL[i])
      return i;
  }
  return -1;
}


void RPackageLister::getFilterNames(vector<string> &filters)
{
    filters.erase(filters.begin(), filters.end());
    
    for (vector<RFilter*>::const_iterator iter = _filterL.begin();
	 iter != _filterL.end();
	 iter++) {
	filters.push_back((*iter)->getName());
    }

}


bool RPackageLister::applyFilters(RPackage *package)
{
    if (_filter == NULL)
	return true;
    
    return _filter->apply(package);
}


void RPackageLister::reapplyFilter()
{
    //cout << "RPackageLister::reapplyFilter():" << _sortMode << endl;
    getFilteredPackages(_displayList);

    // sort now according to the latest used sort method
    switch(_sortMode) {
    case TREE_SORT_NAME:
	sortPackagesByName();    
	break;
    case TREE_SORT_SIZE_ASC:
	sortPackagesByInstSize(0);
	break;
    case TREE_SORT_SIZE_DES:
	sortPackagesByInstSize(1);
	break;
    }
}

// helper function that actually add the pkg to the right location in 
// the tree
void RPackageLister::addFilteredPackageToTree(tree<pkgPair>& pkgTree, 
					      map<string,tree<pkgPair>::iterator>& itermap,
					      RPackage *pkg)
{
    if(pkg == NULL) {
	cerr << "pkg == NULL in addFilteredPackageToTree" << endl;
	cerr << "this shouldn't happen, please report"<< endl;
	return;
    }
    tree<pkgPair>::iterator it;
    
    if(_displayMode == TREE_DISPLAY_SECTIONS) 
	{
	    string sec = pkg->section();
	    if(itermap.find(sec) == itermap.end()) {
#ifndef HAVE_RPM
		string str = sec;
		string suffix;
		// baaaa, special case for stupid debian package naming
		if(str=="non-US/non-free") {
		    str = _("Non US");
		    suffix = _("non free");
		}
		if(str=="non-US/non-free") {
		    str = _("Non US");
		    suffix = _("contrib");
		}
		// if we have something like "contrib/web", make "contrib" the 
		// suffix and translate it independently
		unsigned int n = str.find("/");
		if(n != string::npos) {
		    suffix = str.substr(0,n);
		    str.erase(0,n+1);
		    for(int i=0;transtable[i][0] != NULL;i++) {
			if(suffix == transtable[i][0]) {
			    suffix = _(transtable[i][1]);
			    break;
			}
		    }
		}
		for(int i=0;transtable[i][0] != NULL;i++) {
		    if(str == transtable[i][0]) {
			str = _(transtable[i][1]);
			break;
		    }
		}
		// if we have a suffix, add it
		if(!suffix.empty()) {
		    ostringstream out;
		    ioprintf(out, _("%s <i>(%s)</i>"),
			     str.c_str(), suffix.c_str());
		    str = out.str();
		}
		it = _treeOrganizer.insert(_treeOrganizer.begin(), 
					   pkgPair(str,NULL));
#else
		it = _treeOrganizer.insert(_treeOrganizer.begin(), 
					   pkgPair(sec,NULL));
#endif		
		itermap[sec] = it;
	    } else {
		it = itermap[sec];
	    }
	    pkgPair pair(pkg->name(), pkg);
	    it = _treeOrganizer.append_child(it, pair);
#if 0 // this code was a experiment for a dependencie list under each pkg
	    if((*it).second) {
		vector<RPackage*> deps = ((*it).second)->getInstalledDeps();
		for(int i=0;i<deps.size();i++) {
		    if(deps[i] != NULL) {
			pkgPair pair(deps[i]->name(), deps[i]);
			_treeOrganizer.append_child(it, pair);
		    }
		}
	    }
#endif
	} 
    else if(_displayMode == TREE_DISPLAY_ALPHABETIC) 
	{
	    char s[2] = {0,0};                // get initial letter
	    s[0] = (pkg->name())[0]; 
	    string initial(s);
	    if(itermap.find(initial) == itermap.end()) {
		it = _treeOrganizer.insert(_treeOrganizer.begin(), 
					   pkgPair(initial,NULL));
		itermap[initial] = it;
	    } else {
		it = itermap[initial];
	    }
	    _treeOrganizer.append_child(it, pkgPair(pkg->name(), pkg));
	}
    else if(_displayMode == TREE_DISPLAY_STATUS)
	{
	    string str;
	    int ostatus = pkg->getOtherStatus();
	    if(ostatus & RPackage::ONew)
		str=_("New in repository");
	    else if( (ostatus & RPackage::ONotInstallable) &&
		     !(ostatus & RPackage::OResidualConfig) )
		str=_("Obsolete and locally installed");
	    else if(pkg->installedVersion() != NULL) {
		if(pkg->getStatus() == RPackage::SInstalledOutdated)
		    str=_("Installed and upgradable");
		else
		    str=_("Installed");
	    } else {
		if(pkg->getOtherStatus() & RPackage::OResidualConfig)
		    str=_("Not installed (residual config)");
		else
		    str=_("Not installed");
	    }

	    if(itermap.find(str) == itermap.end()) {
		it = _treeOrganizer.insert(_treeOrganizer.begin(), 
					   pkgPair(str,NULL));
		itermap[str] = it;
	    } else {
		it = itermap[str];
	    }
	    _treeOrganizer.append_child(it, pkgPair(pkg->name(), pkg));
	}

    // _displayMode == TREE_DISPLAY_FLAT) {
    // is handled via the new gtkpkglist code
}

void RPackageLister::getFilteredPackages(vector<RPackage*> &packages)
{    
  map<string,tree<pkgPair>::iterator> itermap;

  //cout << "getFilteredPackages()" << endl;

  if (_updating)
    return;

  packages.erase(packages.begin(), packages.end());
  //cout << "_treeOrganizer.clear()" << endl;
  _treeOrganizer.clear();
  
  for (unsigned i = 0; i < _count; i++) {
    if (applyFilters(_packages[i])) {
	packages.push_back(_packages[i]);
	addFilteredPackageToTree(_treeOrganizer, itermap, _packages[i]);
    }
  }
}


static void qsSortByName(vector<RPackage*> &packages,
			 int start, int end)
{
    int i, j;
    RPackage *pivot, *tmp;
    
    i = start;
    j = end;
    pivot = packages[(i+j)/2];
    do {
	while (strcoll(packages[i]->name(), pivot->name()) < 0) i++;
	while (strcoll(pivot->name(), packages[j]->name()) < 0) j--;
	if (i <= j) {
	    tmp = packages[i];
	    packages[i] = packages[j];
	    packages[j] = tmp;
	    i++;
	    j--;
	}
    } while (i <= j);
    
    if (start < j) qsSortByName(packages, start, j);
    if (i < end) qsSortByName(packages, i, end);
}

void RPackageLister::sortPackagesByName(vector<RPackage*> &packages)
{
    _sortMode = TREE_SORT_NAME;
    if (!packages.empty()) {
	qsSortByName(packages, 0, packages.size()-1);
	_treeOrganizer.sort(_treeOrganizer.begin(), _treeOrganizer.end(),true);
    }
}

// various compare functions
struct instSizeTreeSortFuncAsc {
    bool operator()(const RPackageLister::pkgPair &x, const RPackageLister::pkgPair &y) {
	if(x.second != NULL && y.second != NULL) {
	    return x.second->installedSize() < 
		y.second->installedSize();
	} else
	    // always sort the toplevel by name
	    return x.first < y.first;
    }
};
struct instSizeTreeSortFuncDes {
    bool operator()(const RPackageLister::pkgPair &x, const RPackageLister::pkgPair &y) {
	if(x.second != NULL && y.second != NULL) {
	    return x.second->installedSize() > y.second->installedSize();
	} else 
	    // always sort the toplevel by name
	    return x.first < y.first;
    }
};

struct instSizeSortFuncAsc {
    bool operator()(RPackage *x, RPackage *y) {
	return x->installedSize() < y->installedSize();
    }
};
struct instSizeSortFuncDes {
    bool operator()(RPackage *x, RPackage *y) {
	return x->installedSize() > y->installedSize();
    }
};


void RPackageLister::sortPackagesByInstSize(vector<RPackage*> &packages, int order)
{
    //cout << "RPackageLister::sortPackagesByInstSize()"<<endl;
    if(order == 0)
	_sortMode = TREE_SORT_SIZE_ASC;
    else
	_sortMode = TREE_SORT_SIZE_DES;

    if(!packages.empty()) {
	if(order == 0) {
	    sort(packages.begin(), packages.end(), instSizeSortFuncAsc());
	    _treeOrganizer.sort(_treeOrganizer.begin(), _treeOrganizer.end(),
				instSizeTreeSortFuncAsc(), true);
	}else{
	    sort(packages.begin(), packages.end(), instSizeSortFuncDes());
	    _treeOrganizer.sort(_treeOrganizer.begin(), _treeOrganizer.end(),
				instSizeTreeSortFuncDes(), true);
	}
    }
}

int RPackageLister::findPackage(const char *pattern)
{
    if (_searchData.isRegex)
	regfree(&_searchData.regex);
    
    if (_searchData.pattern)
	free(_searchData.pattern);
    
    _searchData.pattern = strdup(pattern);
    
    if (!_config->FindB("Synaptic::UseRegexp", false) ||
	regcomp(&_searchData.regex, pattern, REG_EXTENDED|REG_ICASE) != 0) {
	_searchData.isRegex = false;
    } else {
	_searchData.isRegex = true;
    }
    _searchData.last = -1;
    
    return findNextPackage();
}


int RPackageLister::findNextPackage()
{
    if (!_searchData.pattern) {
	if (_searchData.last >= (int)_displayList.size())
	    _searchData.last = -1;
	return ++_searchData.last;
    }
    
    int len = strlen(_searchData.pattern);

    for (unsigned i = _searchData.last+1; i < _displayList.size(); i++) {
	if (_searchData.isRegex) {
	    if (regexec(&_searchData.regex, _displayList[i]->name(),
			0, NULL, 0) == 0) {
		_searchData.last = i;
		return i;
	    }
	} else {
	    if (strncasecmp(_searchData.pattern, _displayList[i]->name(), 
			    len) == 0) {
		_searchData.last = i;
		return i;
	    }
	}
    }
    return -1;
}

void RPackageLister::getStats(int &installed, int &broken, 
			      int &toinstall, int &toremove, 
			      double &sizeChange)
{
    pkgDepCache *deps = _cache->deps();

    if (deps != NULL) {
        sizeChange = deps->UsrSize();

        installed = _installedCount;
        broken = deps->BrokenCount();
        toinstall = deps->InstCount();
        toremove = deps->DelCount();
    } else
	sizeChange = installed = broken = toinstall = toremove = 0;
}


void RPackageLister::getDownloadSummary(int &dlCount, double &dlSize)
{
    dlCount = 0;
    dlSize = _cache->deps()->DebSize();
}


void RPackageLister::getSummary(int &held, int &kept, int &essential,
				int &toInstall, int &toUpgrade,	int &toRemove,
				int &toDowngrade, double &sizeChange)
{
    pkgDepCache *deps = _cache->deps();
    unsigned i;

    held = 0;
    kept = deps->KeepCount();
    essential = 0;
    toInstall = 0;
    toUpgrade = 0;
    toDowngrade = 0;
    toRemove = 0;

    for (i = 0; i < _count; i++) {
	RPackage *pkg = _packages[i];

	switch (pkg->getMarkedStatus()) {
	 case RPackage::MKeep:
	    break;
	 case RPackage::MInstall:
	    toInstall++;
	    break;
	 case RPackage::MUpgrade:
	    toUpgrade++;
	    break;
	 case RPackage::MDowngrade:
	     toDowngrade++;
	    break;
	 case RPackage::MRemove:
	    if (pkg->isImportant())
		essential++;
	    toRemove++;
	    break;
	 case RPackage::MHeld:
	    held++;
	    break;
	case RPackage::MBroken:
	case RPackage::MPinned:
	case RPackage::MNew:
	    /* nothing */
	    break;
	}
    }

    sizeChange = deps->UsrSize();
}




struct bla : public binary_function<RPackage*, RPackage*, bool> {
    bool operator()(RPackage *a, RPackage *b) { 
      return strcmp(a->name(), b->name()) < 0; 
    }
};

void RPackageLister::saveUndoState(pkgState &state)
{
    //cout << "RPackageLister::saveUndoState(state)" << endl;
    undoStack.push_front(state);
    redoStack.clear();

#ifdef HAVE_RPM
    unsigned int maxStackSize = _config->FindI("Synaptic::undoStackSize", 3);
#else
    unsigned int maxStackSize = _config->FindI("Synaptic::undoStackSize", 20);
#endif
    while(undoStack.size() > maxStackSize) 
	undoStack.pop_back();
}

void RPackageLister::saveUndoState()
{
    //cout << "RPackageLister::saveUndoState()" << endl;
    pkgState state;
    saveState(state);
    saveUndoState(state);
}


void RPackageLister::undo()
{
    //cout << "RPackageLister::undo()" << endl;
    pkgState state;
    if(undoStack.empty()) {
	//cout << "undoStack empty" << endl;
	return;
    }
    // save redo information
    saveState(state);
    redoStack.push_front(state);

    // undo 
    state = undoStack.front();
    undoStack.pop_front();

    restoreState(state);
}

void RPackageLister::redo()
{
    //cout << "RPackageLister::redo()" << endl;
    pkgState state;
    if(redoStack.empty()) {
	//cout << "redoStack empty" << endl;
	return;
    }
    saveState(state);
    undoStack.push_front(state);

    state = redoStack.front();
    redoStack.pop_front();
    restoreState(state);
}


#ifdef HAVE_RPM
void RPackageLister::saveState(RPackageLister::pkgState &state)
{
    state.Save(_cache->deps());
}

void RPackageLister::restoreState(RPackageLister::pkgState &state)
{
    state.Restore();
}

bool RPackageLister::getStateChanges(RPackageLister::pkgState &state,
				     vector<RPackage*> &toKeep,
				     vector<RPackage*> &toInstall, 
				     vector<RPackage*> &toUpgrade, 
				     vector<RPackage*> &toRemove,
				     vector<RPackage*> &toDowngrade,
				     vector<RPackage*> &exclude,
				     bool sorted)
{
    bool changed = false;

    state.UnIgnoreAll();
    if (exclude.empty() == false) {
	for (vector<RPackage*>::iterator I = exclude.begin();
	     I != exclude.end(); I++)
	    state.Ignore(*(*I)->package());
    }

    pkgDepCache &Cache = *_cache->deps();
    for (unsigned int i = 0; i != _count; i++) {
	RPackage *RPkg = _packages[i];
	pkgCache::PkgIterator &Pkg = *RPkg->package();
	pkgDepCache::StateCache &PkgState = Cache[Pkg];
	if (PkgState.Mode != state[Pkg].Mode &&
	    state.Ignored(Pkg) == false) {
	    if (PkgState.Upgrade()) {
		toUpgrade.push_back(RPkg);
		changed = true;
	    } else if (PkgState.NewInstall()) {
		toInstall.push_back(RPkg);
		changed = true;
	    } else if (PkgState.Delete()) {
		toRemove.push_back(RPkg);
		changed = true;
	    } else if (PkgState.Keep()) {
		toKeep.push_back(RPkg);
		changed = true;
	    } else if (PkgState.Downgrade()) {
		toDowngrade.push_back(RPkg);
		changed = true;
	    }
	}
    }

    if (sorted == true && changed == true) {
	if (toKeep.empty() == false)
	    sort(toKeep.begin(), toKeep.end(), bla());
	if (toInstall.empty() == false)
	    sort(toInstall.begin(), toInstall.end(), bla());
	if (toUpgrade.empty() == false)
	    sort(toUpgrade.begin(), toUpgrade.end(), bla());
	if (toRemove.empty() == false)
	    sort(toRemove.begin(), toRemove.end(), bla());
	if (toDowngrade.empty() == false)
	    sort(toDowngrade.begin(), toDowngrade.end(), bla());
    }

    return changed;
}
#else
void RPackageLister::saveState(RPackageLister::pkgState &state)
{
    //cout << "RPackageLister::saveState()" << endl;
    state.clear();
    state.reserve(_count);
    for (unsigned i = 0; i < _count; i++) {
	state.push_back(_packages[i]->getMarkedStatus());
    }
}

void RPackageLister::restoreState(RPackageLister::pkgState &state)
{
    pkgDepCache *deps = _cache->deps();
    RPackage *pkg;

    for (unsigned i = 0; i < _count; i++) {
	pkg = _packages[i];
	RPackage::MarkedStatus status = pkg->getMarkedStatus();

	if (state[i] != status) {
	    switch (state[i]) {
	    case RPackage::MInstall:
	    case RPackage::MUpgrade:
	    case RPackage::MDowngrade:
		deps->MarkInstall(*(pkg->package()), true);
		break;
		
	    case RPackage::MRemove:
		deps->MarkDelete(*(pkg->package()), false);
		break;
		    
	    case RPackage::MHeld:
	    case RPackage::MKeep:
		deps->MarkKeep(*(pkg->package()), false);
		break;
	    case RPackage::MBroken:
	    case RPackage::MPinned:
	    case RPackage::MNew:
		/* nothing */
		break;
	    }
	}
    }
    notifyChange(NULL);
}

bool RPackageLister::getStateChanges(RPackageLister::pkgState &state,
				     vector<RPackage*> &toKeep,
				     vector<RPackage*> &toInstall, 
				     vector<RPackage*> &toUpgrade, 
				     vector<RPackage*> &toRemove,
				     vector<RPackage*> &toDowngrade,
				     vector<RPackage*> &exclude,
				     bool sorted)
{
    bool changed = false;

    for (unsigned i = 0; i < _count; i++) {
	RPackage::MarkedStatus status = _packages[i]->getMarkedStatus();
	if (state[i] != status && 
	    find(exclude.begin(),exclude.end(),_packages[i])==exclude.end()) {
	    switch (status) {
	    case RPackage::MInstall:
		toInstall.push_back(_packages[i]);
		changed = true;
		break;
		
	    case RPackage::MUpgrade:
		toUpgrade.push_back(_packages[i]);
		changed = true;
		break;
		    
	    case RPackage::MRemove:
		toRemove.push_back(_packages[i]);
		changed = true;
		break;
		
	    case RPackage::MKeep:
	    case RPackage::MHeld:
		toKeep.push_back(_packages[i]);
		changed = true;
		break;

	    case RPackage::MDowngrade:
		toDowngrade.push_back(_packages[i]);
		changed = true;
		break;

	    case RPackage::MBroken:
	    case RPackage::MPinned:
	    case RPackage::MNew:
		/* nothing */
		break;
	    }
	}
    }

    if (sorted == true && changed == true) {
	if (toKeep.empty() == false)
	    sort(toKeep.begin(), toKeep.end(), bla());
	if (toInstall.empty() == false)
	    sort(toInstall.begin(), toInstall.end(), bla());
	if (toUpgrade.empty() == false)
	    sort(toUpgrade.begin(), toUpgrade.end(), bla());
	if (toRemove.empty() == false)
	    sort(toRemove.begin(), toRemove.end(), bla());
	if (toDowngrade.empty() == false)
	    sort(toDowngrade.begin(), toDowngrade.end(), bla());

    }

    return changed;
}
#endif

void RPackageLister::getDetailedSummary(vector<RPackage*> &held, 
					vector<RPackage*> &kept, 
					vector<RPackage*> &essential,
					vector<RPackage*> &toInstall, 
					vector<RPackage*> &toUpgrade, 
					vector<RPackage*> &toRemove,
					vector<RPackage*> &toDowngrade,
					double &sizeChange)
{
  pkgDepCache *deps = _cache->deps();
  unsigned i;
    
  for (i = 0; i < _count; i++) {
    RPackage *pkg = _packages[i];
    
    switch (pkg->getMarkedStatus()) {
    case RPackage::MKeep: {
      if(pkg->getStatus() == RPackage::SInstalledOutdated)
	kept.push_back(pkg);
      break;
    }
    case RPackage::MInstall:
      toInstall.push_back(pkg);
      break;
    case RPackage::MUpgrade:
      toUpgrade.push_back(pkg);
      break;
    case RPackage::MDowngrade:
	toDowngrade.push_back(pkg);
	break;
    case RPackage::MRemove:
      if (pkg->isImportant())
	essential.push_back(pkg);
      else
	toRemove.push_back(pkg);
      break;
    case RPackage::MHeld:
      held.push_back(pkg);
      break;
    case RPackage::MBroken:
    case RPackage::MPinned:
    case RPackage::MNew:
	/* nothing */
	break;
    }
  }

  sort(kept.begin(), kept.end(), bla());
  sort(toInstall.begin(), toInstall.end(), bla());
  sort(toUpgrade.begin(), toUpgrade.end(), bla());
  sort(essential.begin(), essential.end(), bla());
  sort(toRemove.begin(), toRemove.end(), bla());
  sort(held.begin(), held.end(), bla());
  
  sizeChange = deps->UsrSize();
}

bool RPackageLister::updateCache(pkgAcquireStatus *status)
{
    assert(_cache->list() != NULL);
    // Get the source list
    //pkgSourceList List;
#ifdef HAVE_RPM
    _cache->list()->ReadMainList();
#else
    _cache->list()->Read(_config->FindFile("Dir::Etc::sourcelist"));
#endif
    // Lock the list directory
    FileFd Lock;
    if (_config->FindB("Debug::NoLocking",false) == false)
    {
	Lock.Fd(GetLock(_config->FindDir("Dir::State::Lists") + "lock"));
	if (_error->PendingError() == true)
	    return _error->Error(_("Unable to lock the list directory"));
    }
    
    _updating = true;
    
    // Create the download object
    pkgAcquire Fetcher(status);
   
    bool Failed = false;

#if HAVE_RPM
   if (_cache->list()->GetReleases(&Fetcher) == false)
      return false;
   Fetcher.Run();
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); I++)
   {
      if ((*I)->Status == pkgAcquire::Item::StatDone)
	 continue;
      (*I)->Finished();
      Failed = true;
   }
   if (Failed == true)
      _error->Warning(_("Release files for some repositories could not be retrieved or authenticated. Such repositories are being ignored."));
#endif /* HAVE_RPM */

    if (!_cache->list()->GetIndexes(&Fetcher))
       return false;
    
    // Run it
    if (Fetcher.Run() == pkgAcquire::Failed)
	return false;
    
    //bool AuthFailed = false;
    Failed = false;
    for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); 
	 I != Fetcher.ItemsEnd(); 
	 I++)
    {
	if ((*I)->Status == pkgAcquire::Item::StatDone)
	    continue;
	(*I)->Finished();
//	cerr << _("Failed to fetch ") << (*I)->DescURI() << endl;
//	cerr << "  " << (*I)->ErrorText << endl;
	Failed = true;
    }
    
    // Clean out any old list files
    if (_config->FindB("APT::Get::List-Cleanup",true) == true)
    {
	if (Fetcher.Clean(_config->FindDir("Dir::State::lists")) == false ||
	    Fetcher.Clean(_config->FindDir("Dir::State::lists") + "partial/") == false)
	    return false;
    }
    if (Failed == true)
	return _error->Error(_("Some index files failed to download, they have been ignored, or old ones used instead."));

    return true;
}



bool RPackageLister::commitChanges(pkgAcquireStatus *status,
				   RInstallProgress *iprog)
{
    FileFd lock;
    int numPackages = 0;
    bool Ret = true;
    
    _updating = true;
    
    if (!lockPackageCache(lock))
	return false;

    pkgAcquire fetcher(status);

    assert(_cache->list() != NULL);
    // Read the source list
#ifdef HAVE_RPM
    if (_cache->list()->ReadMainList() == false) {
	_userDialog->warning(_("Ignoring invalid record(s) in sources.list file!"));
    }
#else
    if (_cache->list()->Read(_config->FindFile("Dir::Etc::sourcelist")) == false) {
      _userDialog->warning(_("Ignoring invalid record(s) in sources.list file!"));
    }
#endif
    
    _packMan = _system->CreatePM(_cache->deps());
    if (!_packMan->GetArchives(&fetcher, _cache->list(), _records) ||
	_error->PendingError())
	goto gave_wood;

    // ripped from apt-get
    while (1) {
	bool Transient = false;
	
	if (fetcher.Run() == pkgAcquire::Failed)
	    goto gave_wood;
	
        string serverError;

	// Print out errors
	bool Failed = false;
	for (pkgAcquire::ItemIterator I = fetcher.ItemsBegin();
	     I != fetcher.ItemsEnd(); 
	     I++) {
	    if ((*I)->Status == pkgAcquire::Item::StatDone &&
		(*I)->Complete)
	    {
		numPackages += 1;
		continue;
	    }
	    
	    if ((*I)->Status == pkgAcquire::Item::StatIdle)
	    {
		Transient = true;
		continue;
	    }
	    
	    string errm = (*I)->ErrorText;
	    string tmp = _("Failed to fetch ") + (*I)->DescURI() + "\n"
		"  " + errm;
	   
	    serverError = getServerErrorMessage(errm);

	    _error->Warning(tmp.c_str());
	    Failed = true;
	}

	if (_config->FindB("Synaptic::Download-Only", false)) {
	    _updating = false;
	    return !Failed;
	}

	if (Failed) {
	    string message;
	   
	    if (Transient)
		goto gave_wood;

	    message = _("Some of the packages could not be retrieved from the server(s).\n");
	    if (!serverError.empty())
	       message += "("+serverError+")\n";
	    message += _("Do you want to continue, ignoring these packages?");

	    if (!_userDialog->confirm(message.c_str()))
		goto gave_wood;
	}
	// Try to deal with missing package files
	if (Failed == true && _packMan->FixMissing() == false) {
	    _error->Error(_("Unable to correct missing packages"));
	    goto gave_wood;
	}
       
        // need this so that we first fetch everything and then install (for CDs)
	if (Transient == false || _config->FindB("Acquire::CDROM::Copy-All", false) == false) {

	    if (Transient == true) {
		// We must do that in a system independent way. */
		_config->Set("RPM::Install-Options::", "--nodeps");
	    }
	    
	    _cache->releaseLock();

	    pkgPackageManager::OrderResult Res = iprog->start(_packMan, numPackages);
	    if (Res == pkgPackageManager::Failed || _error->PendingError()) {
		if (Transient == false)
		    goto gave_wood;
		Ret = false;
		// TODO: We must not discard errors here. The right
		//       solution is to use an "error stack", as
		//       implemented in apt-rpm.
		//_error->DumpErrors();
		_error->Discard();
	    }
	    if (Res == pkgPackageManager::Completed)
		break;

	    numPackages = 0;

	    _cache->lock();
	}
	
	// Reload the fetcher object and loop again for media swapping
	fetcher.Shutdown();

	if (!_packMan->GetArchives(&fetcher, _cache->list(), _records))
	    goto gave_wood;
    }

    //cout << _("Finished.")<<endl;

    
    // erase downloaded packages
    cleanPackageCache();

    delete _packMan;
    return Ret;
    
gave_wood:
    delete _packMan;
    
    return false;
}


bool RPackageLister::lockPackageCache(FileFd &lock)
{
    // Lock the archive directory
    
    if (_config->FindB("Debug::NoLocking",false) == false)
    {
	lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
	if (_error->PendingError() == true)
	    return _error->Error(_("Unable to lock the download directory"));
    }
    
    return true;
}


bool RPackageLister::cleanPackageCache(bool forceClean)
{
    FileFd lock;
    
    if (_config->FindB("Synaptic::CleanCache", false) || forceClean) {

	lockPackageCache(lock);
	
	pkgAcquire Fetcher;
	Fetcher.Clean(_config->FindDir("Dir::Cache::archives"));
	Fetcher.Clean(_config->FindDir("Dir::Cache::archives") + "partial/");
    } else if (_config->FindB("Synaptic::AutoCleanCache", false)) {

	lockPackageCache(lock);
   
	pkgArchiveCleaner cleaner;

	bool res;

	res = cleaner.Go(_config->FindDir("Dir::Cache::archives"), 
			 *_cache->deps());
	
	if (!res)
	    return false;
	
	res = cleaner.Go(_config->FindDir("Dir::Cache::archives") + "partial/",
			 *_cache->deps());

	if (!res)
	    return false;

    } else {
	// mvo: nothing?
    }
    
    return true;
}


bool RPackageLister::writeSelections(ostream &out, bool fullState)
{
    //cout << "bool RPackageLister::writeSelections(out,fullState)"<<endl;

    for (unsigned i = 0; i < _count; i++) {
	RPackage::MarkedStatus mstatus = _packages[i]->getMarkedStatus();
	RPackage::PackageStatus status = _packages[i]->getStatus();
	switch (mstatus) {
	    case RPackage::MInstall:
	    case RPackage::MUpgrade:
		out << _packages[i]->name() << "   \t   install" << endl;
		// marked state has priority so we skip the rest
		continue;
		break;
	    case RPackage::MRemove:
		out << _packages[i]->name() << "   \t   deinstall" << endl;
		// marked state has priority so we skip the rest
		continue;
		break;
	    case RPackage::MHeld:
	    case RPackage::MKeep:
	    case RPackage::MDowngrade:
	    case RPackage::MBroken:
	    case RPackage::MPinned:
	    case RPackage::MNew:
		/* nothing */
		break;
	}
	// full state saves all installed packages
	if(fullState) {
	    switch(status) {
	    case RPackage::SInstalledUpdated:
	    case RPackage::SInstalledOutdated:
		out << _packages[i]->name() << "   \t   install" << endl;
		break;
	    }
	}
    }
    return true;
}

bool RPackageLister::readSelections(istream &in)
{
    char Buffer[300];
    int CurLine = 0;
    enum Action {
	ACTION_INSTALL,
	ACTION_UNINSTALL
    };
    map<string,int> actionMap;

    while (in.eof() == false)
    {
        in.getline(Buffer,sizeof(Buffer));
        CurLine++;

        if (in.fail() && !in.eof())
            return _error->Error(_("Line %u too long in selection file."),
                                 CurLine);

        _strtabexpand(Buffer,sizeof(Buffer));
        _strstrip(Buffer);
      
        const char *C = Buffer;
      
        // Comment or blank
        if (C[0] == '#' || C[0] == 0)
	        continue;

        string PkgName;
        if (ParseQuoteWord(C,PkgName) == false)
	        return _error->Error(_("Malformed line %u in selection file"),
			                     CurLine);
        string Action;
        if (ParseQuoteWord(C,Action) == false)
	        return _error->Error(_("Malformed line %u in selection file"),
			                     CurLine);

	if (Action[0] == 'i') {
	    actionMap[PkgName] = ACTION_INSTALL;
	} else if (Action[0] == 'u' || Action[0] == 'd') {
	    actionMap[PkgName] = ACTION_UNINSTALL;
	}
    }

    if (actionMap.empty() == false) {
	int Size = actionMap.size();
	_progMeter->OverallProgress(0,Size,Size,_("Setting selections..."));
	_progMeter->SubProgress(Size);
	pkgDepCache &Cache = *_cache->deps();
	pkgProblemResolver Fix(&Cache);
	// XXX Should protect whatever is already selected in the cache.
	pkgCache::PkgIterator Pkg;
	int Pos = 0;
	for (map<string,int>::const_iterator I = actionMap.begin();
	     I != actionMap.end(); I++) {
	    Pkg = Cache.FindPkg((*I).first);
	    if (Pkg.end() == false) {
		Fix.Clear(Pkg);
		Fix.Protect(Pkg);
		switch ((*I).second) {
		    case ACTION_INSTALL:
			Cache.MarkInstall(Pkg, true);
			break;

		    case ACTION_UNINSTALL:
			Fix.Remove(Pkg);
			Cache.MarkDelete(Pkg, false);
			break;
		}
            }
	    if ((Pos++)%5 == 0)
		_progMeter->Progress(Pos);
        }
	_progMeter->Done();
	Fix.InstallProtect();
	Fix.Resolve(true);
    }
    
    return true;
}

// vim:sts=4:sw=4
