/* rpackage.h - wrapper for accessing package information
 * 
 * Copyright (c) 2000, 2001 Conectiva S/A 
 *               2002 Michael Vogt <mvo@debian.org>
 * 
 * Author: Alfredo K. Kojima <kojima@conectiva.com.br>
 *         Michael Vogt <mvo@debian.org>
 * 
 * Portions Taken from Gnome APT
 *   Copyright (C) 1998 Havoc Pennington <hp@pobox.com>
 * 
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



#ifndef _RPACKAGE_H_
#define _RPACKAGE_H_

#include <vector>

#include <apt-pkg/pkgcache.h>

using namespace std;

class pkgDepCache;
class RPackageLister;
class pkgRecords;

enum {NO_PARSER, DEB_PARSER, STRIP_WS_PARSER, RPM_PARSER};

class RPackage {
   RPackageLister *_lister;
   
   pkgRecords *_records;
   pkgDepCache *_depcache;
   pkgCache::PkgIterator *_package;
   // save the default candidate version to undo version selection
   const char *_candidateVer;

   bool _newPackage;
   bool _pinnedPackage;
   bool _orphanedPackage;
   bool _purge;

   enum ChangeReason {unknown, manual, weak_depends, libapt};
   ChangeReason last_change;

   bool _notify;

   // virtual pkgs provided by this one
   vector<pkgCache::PkgIterator> _virtualPackages; 

   // provides list as string
   vector<const char*> _provides;
   
   // stuff for enumerators
   int _vpackI;
   pkgCache::DepIterator _rdepI;
   
   pkgCache::DepIterator _wdepI;
   pkgCache::DepIterator _wdepStart;
   pkgCache::DepIterator _wdepEnd;
   
   pkgCache::DepIterator _depI;
   pkgCache::DepIterator _depStart;
   pkgCache::DepIterator _depEnd;

   bool isShallowDependency(RPackage *pkg);

   bool isWeakDep(pkgCache::DepIterator &dep);
   
public:
   enum PackageStatus {
       SInstalledUpdated,
       SInstalledOutdated,
       SInstalledBroken, // installed but broken	   
       SNotInstalled
   };
   
   enum MarkedStatus {
       MKeep,
       MInstall,
       MUpgrade,
       MDowngrade,
       MRemove,
       MHeld,
       MBroken
   };

   enum OtherStatus {
     OOrphaned        = 1<<0,
     OPinned          = 1<<1, /* apt-pined */
     ONew             = 1<<2,
     OResidualConfig  = 1<<3,
     ONotInstallable  = 1<<4,
     OPurge           = 1<<5
   };
   
   enum UpdateImportance {
       IUnknown,
       INormal,
       ICritical,
       ISecurity
   };
   
   pkgCache::PkgIterator *package() { return _package; };
   

   inline const char *name() { return _package->Name(); };
   
   const char *section();
   const char *priority();

   const char *summary();
   const char *description();
   const char *installedFiles();
#ifdef HAVE_DEBTAGS
   const char *tags();
#endif
   vector<const char*> provides(); 

   // get all available versions (version, release)
   vector<pair<string,string> > getAvailableVersions();

   bool isImportant();

   const char *maintainer();
   const char *vendor();
   
   const char *installedVersion();
   long installedSize();

   // if this is an update
   UpdateImportance updateImportance();
   const char *updateSummary();
   const char *updateDate();
   const char *updateURL();

   // relative to version that would be installed
   const char *availableVersion();
   long availableInstalledSize();
   long availablePackageSize();

   // special case: alway get the deps of the latest available version
   // (not necessary the installed one)
   bool enumAvailDeps(const char *&type, const char *&what, const char *&pkg,
		 const char *&which, char *&summary, bool &satisfied);

   // this gives the dependencies for the installed package
   vector<RPackage*> getInstalledDeps();

   // installed package if installed, scheduled/candidate if not or if marked
   bool enumDeps(const char *&type, const char *&what, const char *&pkg,
		 const char *&which, char *&summary, bool &satisfied);
   bool nextDeps(const char *&type, const char *&what, const char *&pkg,
		 const char *&which, char *&summary, bool &satisfied);

   // does the pkg depends on this one?
   bool dependsOn(const char *pkgname);

   // reverse dependencies
   bool enumRDeps(const char *&dep, const char *&what);
   bool nextRDeps(const char *&dep, const char *&what);
   
   // weak dependencies
   bool enumWDeps(const char *&type, const char *&what, bool &satisfied);
   bool nextWDeps(const char *&type, const char *&what, bool &satisfied);

   // current status query
   PackageStatus getStatus();

   // selected status query
   MarkedStatus getMarkedStatus();

   // other information about the package (bitwise encoded in the returned int)
   int getOtherStatus();

   bool wouldBreak();

   void inline setNew(bool isNew=true) { _newPackage=isNew; };
   void setPinned(bool flag);
   void setOrphaned(bool flag=true) { _orphanedPackage=flag; };

   // change status
   void setKeep();
   void setInstall();
   void setRemove(bool purge = false); //XXX: purge for debian

   void setNotify(bool flag=true);

   // shallow doesnt remove things other pkgs depend on
   void setRemoveWithDeps(bool shallow, bool purge=false);

   void addVirtualPackage(pkgCache::PkgIterator dep);

   // install verTag version
   bool setVersion(const char* verTag);
   // cancel version selection
   void unsetVersion() { setVersion(_candidateVer); };
   string showWhyInstBroken();

   RPackage(RPackageLister *lister, pkgDepCache *depcache, pkgRecords *records,
	    pkgCache::PkgIterator &pkg);
   ~RPackage();
};

#endif
