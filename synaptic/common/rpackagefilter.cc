/* rpackagefilter.cc - filters for package listing
 * 
 * Copyright (c) 2000-2003 Conectiva S/A 
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
#include "i18n.h"

#include <iostream>
#include <algorithm>
#include <cstdio>
#include <fnmatch.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>

#include "rpackagefilter.h"
#include "rpackagelister.h"
#include "rpackage.h"

using namespace std;

const char *RPFStatus = _("Status");
const char *RPFPattern = _("Pattern");
const char *RPFSection = _("Section");
const char *RPFPriority = _("Priority");
const char *RPFReducedView = _("ReducedView");


#ifdef HAVE_DEBTAGS
const char *RPFTags = _("Tags");

bool RTagPackageFilter::read(Configuration &conf, string key)
{
    const Configuration::Item *top;
    
    reset();
    
    top = conf.Tree(string(key+"::include").c_str());

    if(top == NULL)
	return true;

    for (top = top->Child; top != NULL; top = top->Next) {
	include(top->Value);
    }
    
    top = conf.Tree(string(key+"::exclude").c_str());

    if(top == NULL)
	return true;

    for (top = top->Child; top != NULL; top = top->Next) {
	exclude(top->Value);
    }


    return true;

}

bool RTagPackageFilter::write(ofstream &out, string pad)
{

    out << pad + "include {" << endl;
    if(!included.empty()) {

	for (OpSet<string>::const_iterator it=included.begin();
	     it != included.end(); it++) {
	    out << pad + "  \"" << *it << "\"; " << endl;
	}
    }
    out << pad + "};" << endl;


    out << pad + "exclude {" << endl;
    if(!excluded.empty()) {

	for (OpSet<string>::const_iterator it=excluded.begin();
	     it != excluded.end(); it++) {
	    out << pad + "  \"" << *it << "\"; " << endl;
	}
    }
    out << pad + "};" << endl;


    return true;

}
#endif

int RSectionPackageFilter::count() 
{
    return _groups.size(); 
}


bool RSectionPackageFilter::inclusive()
{
    return _inclusive; 
}


string RSectionPackageFilter::section(int index)
{
    return _groups[index]; 
}


void RSectionPackageFilter::clear()
{ 
    _groups.erase(_groups.begin(), _groups.end());
}


bool RSectionPackageFilter::filter(RPackage *pkg)
{
    string sec;
    
    if (pkg->section() == NULL)
        return _inclusive ? false : true;
    
    sec = pkg->section();
    
    for (vector<string>::const_iterator iter = _groups.begin();
	 iter != _groups.end(); iter++) {
	if (sec == (*iter)) {
	    return _inclusive ? true : false;
	}
    }
    
    return _inclusive ? false : true;
}


bool RSectionPackageFilter::write(ofstream &out, string pad)
{
    out << pad + "inclusive " << (_inclusive ? "true;" : "false;") << endl;

    out << pad + "sections {" << endl;
    
    for (int i = 0; i < count(); i++) {
	out << pad + "  \"" << section(i) << "\"; " << endl;
    }
    
    out << pad + "};" << endl;
    return true;
}


bool RSectionPackageFilter::read(Configuration &conf, string key)
{
    const Configuration::Item *top;
    
    reset();

    _inclusive = conf.FindB(key+"::inclusive", _inclusive);
    
    top = conf.Tree(string(key+"::sections").c_str());

    // mvo: FIXME this sucks
    if(top == NULL) {
      return true;
    }

    for (top = top->Child; top != NULL; top = top->Next) {
	addSection(top->Value);
    }
    
    return true;
}

// don't translate this, they are only used in the filter file
char *RPatternPackageFilter::TypeName[] = {
    N_("Name"),
    N_("Version"),
    N_("Description"),
    N_("Maintainer"),
    N_("Depends"),
    N_("Provides"),
    N_("Conflicts"),
    N_("Replaces"),
    N_("WeakDepends"),
    N_("ReverseDepends")
};


bool RPatternPackageFilter::filter(RPackage *pkg)
{
    bool found;
    bool globalfound = false;
    bool useregexp = _config->FindB("Synaptic::UseRegexp", false);

    if (_patterns.size() == 0)
	return true;

    for (vector<Pattern>::const_iterator iter = _patterns.begin();
	 iter != _patterns.end(); iter++) 
	{
	    found = true;
	    if (iter->where == Name) {
		const char *name = pkg->name();
// if we want "real" nouseregexp support, we need to split the string
// here like we do with regexp
//		if(!useregexp) {
//		    if(strcasestr(name, iter->pattern.c_str()) == NULL) {
//			found = false;
//		    } 
		for(unsigned int i=0;i<iter->regexps.size();i++) {
		    if(regexec(iter->regexps[i], name, 0, NULL, 0) == 0)
			found &= true;
		    else 
			found = false;
		}
	    } else if (iter->where == Version) {
		const char *version = pkg->availableVersion();
		if(version == NULL) {
		    found = false;
		} else {
		    for(unsigned int i=0;i<iter->regexps.size();i++) {
			if(regexec(iter->regexps[i], version, 0, NULL, 0) == 0)
			    found &= true;
			else 
			    found = false;
		    }
		}
	    } else if (iter->where == Description) {
		const char *s1 = pkg->summary();
		const char *s2 = pkg->description();
		for(unsigned int i=0;i<iter->regexps.size();i++) {
		    if(regexec(iter->regexps[i], s1, 0, NULL, 0) == 0) {
			found &= true;
		    } else {
			if(regexec(iter->regexps[i], s2, 0, NULL, 0) == 0)
			    found &= true;
			else 
			    found = false;
		    }
		}
	    } else if (iter->where == Maintainer) {
		const char *maint = pkg->maintainer();
		for(unsigned int i=0;i<iter->regexps.size();i++) {
		    if(regexec(iter->regexps[i], maint, 0, NULL, 0) == 0) {
			found &= true;
		    } else {
			found = false;
		    }
		}
	    } else if (iter->where == Depends) {
		const char *depType, *depPkg, *depName, *depVer;
		char *summary;
		bool ok;
		found = false;
		if(pkg->enumAvailDeps(depType, depName, depPkg, depVer, summary, ok))
		    {
			do 
			    {
				if(strstr(depType,"Depends") != NULL)
				    if(regexec(iter->regexps[0], depName, 0, NULL, 0) == 0) {
					    found = true;
					    break;
				    }
  			    } while(pkg->nextDeps(depType, depName, depPkg, depVer, summary, ok));
		    } 
	    } else if (iter->where == Provides) {
		found = false;
		vector<const char*> provides = pkg->provides();
		for(unsigned int i=0;i<provides.size();i++) {
		    if(regexec(iter->regexps[0], provides[i], 0, NULL, 0) == 0) {
			    found = true;
			    break;
		    }
		}
	    } else if (iter->where == Conflicts) {
		const char *depType, *depPkg, *depName, *depVer;
		char *summary;
		bool ok;
		found = false;
		if(pkg->enumAvailDeps(depType, depName, depPkg, depVer, summary, ok))
		    {
			do 
			    {
				if(strstr(depType,"Conflicts") != NULL)
				    if(regexec(iter->regexps[0], depName, 0, NULL, 0) == 0) {
					    found = true;
					    break;
				    }
			    } while(pkg->nextDeps(depType, depName, depPkg, depVer, summary, ok));
		    }
	    } else if (iter->where == Replaces) {
		const char *depType, *depPkg, *depName, *depVer;
		char *summary;
		bool ok;
		found = false;
		if(pkg->enumAvailDeps(depType, depName, depPkg, depVer, summary, ok))
		    {
			do 
			    {
				if(strstr(depType,"Replaces") != NULL)
				    if(regexec(iter->regexps[0], depName, 0, NULL, 0) == 0) {
					found = true;
					break;
				    } 
			    } while(pkg->nextDeps(depType, depName, depPkg, depVer, summary, ok));
		    }
 
	    } else if (iter->where == WeakDepends) {
		found = false;
		const char *depType, *depPkg, *depName;
		bool ok;
		if (pkg->enumWDeps(depType, depPkg, ok)) {
		    do {
			if(regexec(iter->regexps[0], depPkg, 0, NULL, 0) == 0) {
			    found = true;
			    break;
			} 
		    } while (pkg->nextWDeps(depName, depPkg, ok));
		}
	    } else if (iter->where == RDepends) {
		found = false;
		const char *depPkg, *depName;
		if (pkg->enumRDeps(depName, depPkg)) {
		    do {
			if(regexec(iter->regexps[0], depName, 0, NULL, 0) == 0) {
				found = true;
			}
		    } while (pkg->nextRDeps(depName, depPkg));
		}
		
	    }

// 	    // give back all the memory of the regexps
// 	    for(unsigned int i=0;i<regexps.size();i++)
// 		regfree(regexps[i]);
// 	    regexps.clear();

// 	    if(found) {
// 		cout << "pkg: " << pkg->name() 
// 		     << " pattern: " << iter->pattern 
// 		     << " found: " << found << " " << globalfound 
// 		     << endl;
// 	    }
//  	    if(strstr(pkg->name(),"bsd-ftpd")) {
//  		cout << "bsd-ftpd ... pkg: " << pkg->name() 
//  		     << " maint: " << maint 
//  		     << endl;
//  	    }

	    // match the first "found", 
	    // otherwise look if it was a exclusive rule
	    if(found) 
		return !iter->exclusive;
	    else
		globalfound |= iter->exclusive;
	}

    return globalfound;
}


void RPatternPackageFilter::addPattern(DepType type, string pattern, 
				       bool exclusive)
{
  //cout << "adding pattern: " << pattern << endl;
    Pattern pat;
    pat.where = type;
    pat.pattern = pattern; 
    pat.exclusive = exclusive;

    // compile the regexps
    string S;
    const char *C = pattern.c_str();

    vector<regex_t*> regexps;
    while (*C != 0) {
	if (ParseQuoteWord(C, S) == true) {
	    regex_t *reg = new regex_t;
	    if(regcomp(reg, S.c_str(), REG_EXTENDED|REG_ICASE|REG_NOSUB) != 0)
		{
		    cerr << "regexp compilation error" << endl;
		    for(unsigned int i=0;i<regexps.size();i++) {
			regfree(regexps[i]);
		    }
		    return;
		}
	    regexps.push_back(reg);
	}
    }
    pat.regexps = regexps;

    _patterns.push_back(pat);
}


bool RPatternPackageFilter::write(ofstream &out, string pad)
{
    DepType type;
    string pat;
    bool excl;

    out << pad + "patterns {" << endl;
    
    for (int i = 0; i < count(); i++) {
	getPattern(i, type, pat, excl);
	out << pad + "  " + TypeName[(int)type] + ";"
	    << " \"" << pat << "\"; "  
	    << (excl ? "true;" : "false;") << endl;
    }
    
    out << pad + "};" << endl;
    return true;
}


bool RPatternPackageFilter::read(Configuration &conf, string key)
{
    const Configuration::Item *top;
    DepType type;
    string pat;
    bool excl;

    top = conf.Tree(string(key+"::patterns").c_str());
    if(top == NULL)
      return true;

    top = top->Child;
    while (top) {
	int i;
	for (i = 0; top->Value != TypeName[i]; i++);
	type = (DepType)i;
	top = top->Next;
	pat = top->Value;
	top = top->Next;
	excl = top->Value == "true";
	top = top->Next;
	
	addPattern(type, pat, excl);
    }

    return true;
}

// copy constructor
RPatternPackageFilter::RPatternPackageFilter(RPatternPackageFilter &f)
{
    //cout << "RPatternPackageFilter(&RPatternPackageFilter f)" << endl;
    for(int i=0;i<f._patterns.size();i++) {
	addPattern(f._patterns[i].where,
		   f._patterns[i].pattern,
		   f._patterns[i].exclusive);
    }
}

void RPatternPackageFilter::clear()
{
    //cout << "void RPatternPackageFilter::clear()" << endl;
    //for(int i=0;i<count();i++) {
    //	cout << "\t " << _patterns[i].pattern << endl;
    //}
    
    // give back all the memory
    for(int i=0;i<_patterns.size();i++) {
	for(int j=0;j<_patterns[i].regexps.size();j++) {
	    regfree(_patterns[i].regexps[j]);
	    delete (regex_t*)_patterns[i].regexps[j];
	}
    }

    _patterns.erase(_patterns.begin(), _patterns.end());
}


RPatternPackageFilter::~RPatternPackageFilter()
{
    //cout << "RPatternPackageFilter::~RPatternPackageFilter()" << endl;
    
    this->clear();
}




bool RStatusPackageFilter::filter(RPackage *pkg)
{
    RPackage::PackageStatus status = pkg->getStatus();
    RPackage::MarkedStatus marked = pkg->getMarkedStatus();
    int ostatus = pkg->getOtherStatus();

    if (_status & MarkKeep) {
	if (marked == RPackage::MKeep)
	    return true;
    }
    
    if (_status & MarkInstall) {
	if (marked == RPackage::MInstall || 
	    marked == RPackage::MUpgrade ||
	    marked == RPackage::MDowngrade)
	    return true;
    }
    
    if (_status & MarkRemove) {
	if (marked == RPackage::MRemove)
	    return true;
    }


    if (_status & Installed) {
	if (status != RPackage::SNotInstalled)
	    return true;
    }
    
    if (_status & NotInstalled) {
	if (status == RPackage::SNotInstalled)
	    return true;
    }
    
    if (_status & Broken) {
	if (status == RPackage::SInstalledBroken || pkg->wouldBreak())
	    return true;
    }
    
    if (_status & Upgradable) {
	if (status == RPackage::SInstalledOutdated)
	    return true;
    }

    if(_status & NewPackage ) {
      if(ostatus & RPackage::ONew) {
	//cout << "pkg(" << pkg->name() << ")->isNew()" << endl;
	return true;
      }
    }

    if(_status & OrphanedPackage ) {
      if(ostatus & RPackage::OOrphaned) {
	//cout << "pkg(" << pkg->name() << ")->isOrphaned()" << endl;
	return true;
      }
    }

    if(_status & PinnedPackage ) {
      if(ostatus & RPackage::OPinned) {
	return true;
      }
    }

    if(_status & ResidualConfig ) {
      if(ostatus & RPackage::OResidualConfig) {
	return true;
      }
    }

    if(_status & NotInstallable ) {
      if(ostatus & RPackage::ONotInstallable) {
	  //cout << "pkg(" << pkg->name() << ")->isNotInRepository()" << endl;
	  return true;
      }
    }


    return false;
}


bool RStatusPackageFilter::write(ofstream &out, string pad)
{
    char buf[16];
    
    snprintf(buf, sizeof(buf), "0x%x", _status);
    
    out << pad + "flags" + " " << buf << ";"<< endl;
    return true;
}

bool RStatusPackageFilter::read(Configuration &conf, string key)
{
    _status = conf.FindI(key+"::flags", 0xffffff);
    return true;
}


bool RPriorityPackageFilter::filter(RPackage *pkg)
{
    return true;
}

// mvo: FIXME!
bool RPriorityPackageFilter::write(ofstream &out, string pad)
{
    return true;
}

// mvo: FIXME!
bool RPriorityPackageFilter::read(Configuration &conf, string key)
{
    return true;
}

RReducedViewPackageFilter::~RReducedViewPackageFilter()
{
    if (_hide_regex.empty() == false) {
       for (vector<regex_t*>::const_iterator I = _hide_regex.begin();
	    I != _hide_regex.end(); I++) {
	  delete *I;
       }
    }
}

// Filter out all packages for which there are ShowDependencies that
// are not installed.
bool RReducedViewPackageFilter::filter(RPackage *pkg)
{
    const char *name = pkg->name();
    if (_hide.empty() == false
        && _hide.find(name) != _hide.end())
       return false;
    if (_hide_wildcard.empty() == false) {
       for (vector<string>::const_iterator I = _hide_wildcard.begin();
	    I != _hide_wildcard.end(); I++) {
	  if (fnmatch(I->c_str(), name, 0) == 0)
	     return false;
       }
    }
    if (_hide_regex.empty() == false) {
       for (vector<regex_t*>::const_iterator I = _hide_regex.begin();
	    I != _hide_regex.end(); I++) {
	  if (regexec(*I, name, 0, 0, 0) == 0)
	     return false;
       }
    }
    return true;
}

void RReducedViewPackageFilter::addFile(string FileName)
{
   FileFd F(FileName, FileFd::ReadOnly);
   if (_error->PendingError()) 
   {
      _error->Error(_("Internal Error: Could not open ReducedView file %s"),
		    FileName.c_str());
      return;
   }
   pkgTagFile Tags(&F);
   pkgTagSection Section;

   string S;
   const char *C;
   while (Tags.Step(Section)) {
      string Name = Section.FindS("Name");
      string Match = Section.FindS("Match");
      string ReducedView = Section.FindS("ReducedView");
      if (Name.empty() == true || ReducedView.empty() == true)
	 continue;

      C = ReducedView.c_str();
      if (ParseQuoteWord(C,S) && S == "hide") {
	 if (Match.empty() == true || Match == "exact") {
	    _hide.insert(Name);
	 } else if (Match == "wildcard") {
	    _hide_wildcard.push_back(Name);
	 } else if (Match == "regex") {
	    regex_t *ptrn = new regex_t;
	    if (regcomp(ptrn,Name.c_str(),
			REG_EXTENDED|REG_ICASE|REG_NOSUB) != 0)
	    {
	       _error->Warning(_("Bad regular expression '%s' in ReducedView file."),
			       Name.c_str());
	       delete ptrn;
	    }
	    else
	       _hide_regex.push_back(ptrn);
	 }
      }
   }
}

bool RReducedViewPackageFilter::write(ofstream &out, string pad)
{
    out << pad + "enabled " << (_enabled?"true":"false") << ";"<< endl;
    return true;
}

bool RReducedViewPackageFilter::read(Configuration &conf, string key)
{
    _enabled = conf.FindB(key+"::enabled");
    if (_enabled == true) {
	string FileName = _config->Find("Synaptic::ReducedViewFile",
					"/etc/apt/metadata");
	if (FileExists(FileName))
	    addFile(FileName);
    }
    return true;
}


bool RFilter::apply(RPackage *package)
{
    if (!section.filter(package))
	return false;

    if (!status.filter(package))
	return false;

    if (!pattern.filter(package))
	return false;

    if (!priority.filter(package))
	return false;
#ifdef HAVE_DEBTAGS
    if(!tags.filter(package))
	return false;
#endif
    if (!reducedview.filter(package))
	return false;

    return true;
}


void RFilter::reset()
{
    section.reset();
    status.reset();
    pattern.reset();
    priority.reset();
#ifdef HAVE_DEBTAGS
    tags.reset();
#endif
    reducedview.reset();
}

void RFilter::setName(string s)
{
  if(s.empty()) {
    cerr << "Internal Error: empty filter name!? should _never_ happen, please report" << endl;
    name =_("unkown");
  } else {
    if(s.length() > 55) {
      cerr << "Internal Error: filter name is longer than 55 chars!? "
	      "Will be truncated.Please report" << endl;
      s.resize(55);
      name = s;
    } else {
      name = s;
    }
  }
}

string RFilter::getName()
{
  // Return name with i18n conversion. Filters names are saved without i18n.
  return _(name.c_str());
}

bool RFilter::read(Configuration &conf, string key)
{
    bool res = true;
    
    //cout << "reading filter "<< name << endl;
    
    res &= section.read(conf, key+"::section");
    res &= status.read(conf, key+"::status");
    res &= pattern.read(conf, key+"::pattern");
    res &= priority.read(conf, key+"::priority");
#ifdef HAVE_DEBTAGS
    res &= tags.read(conf, key+"::tags");
#endif
    res &= reducedview.read(conf, key+"::reducedview");

    _view.viewMode=conf.FindI(key+"::view::viewMode", 0);
    _view.expandMode=conf.FindI(key+"::view::expandMode", 0);

    return res;
}


bool RFilter::write(ofstream &out)
{
    bool res = true;

    //cout <<"writing filter: \""<<name<<"\""<<endl;

    if (name.empty()) {
	if (getenv("DEBUG_SYNAPTIC"))
	    abort();
	
	name = "?";
    }
    
    out << "filter \"" << name << "\" {" <<endl;
    string pad = "  ";
    
    out << pad+"section {" << endl;
    res &= section.write(out, pad+"  ");
    out << pad+"};" << endl;
    
    out << pad+"status {" << endl;
    res &= status.write(out, pad+"  ");
    out << pad+"};" << endl;
    
    out << pad+"pattern {" << endl;
    res &= pattern.write(out, pad+"  ");
    out << pad+"};" << endl;
    
    out << pad+"priority {" << endl;
    res &= priority.write(out, pad+"  ");
    out << pad+"};" << endl;
#ifdef HAVE_DEBTAGS
    out << pad+"tags {" << endl;
    res &= tags.write(out, pad+"  ");
    out << pad+"};" << endl;
#endif
    out << pad+"reducedview {" << endl;
    res &= reducedview.write(out, pad+"  ");
    out << pad+"};" << endl;
   
    out << pad+"view {" << endl;
    out << pad+pad+"viewMode " << _view.viewMode << ";" << endl;
    out << pad+pad+"expandMode " << _view.expandMode << ";" << endl;
    out << pad+"};" << endl;
    
    out << "};" << endl;

    return res;
}

// vim:sts=3:sw=3
