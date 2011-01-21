# hey Emacs, its -*- mode: rpm-spec; coding: cyrillic-cp1251; -*-
# $Id: synaptic,v 1.1 2003/04/28 11:30:54 svd Exp $

# '--with ru_po' if we have updated ru.po (Source1)
%def_with ru_po
# '--with ru_man' if we have updated russian manpage (Source2)
%def_without ru_man
%def_enable autotools

%define rel %nil
Name: synaptic
Version: 0.57.3
Release: alt1

Summary: Graphical front-end for APT
Summary(ru_RU.CP1251): ����������� �������� ��� APT
Group: System/Configuration/Packaging
License: GPL
Url: http://www.nongnu.org/synaptic/
Packager: Sviatoslav Sviridov <svd@altlinux.ru>

Source: http://people.debian.org/~mvo/synaptic/%name-%version%rel.tar.gz
Source1: %name-ru.po
Source2: %name.ru.8
Source3: package-supported.png
Source4: %name.conf

# This patch needed to build synaptic with apt < 0.5.5cnc5 only
Patch1: synaptic-0.36-alt-state.patch

Patch2: synaptic-0.47-alt-xslt.patch
Patch3: synaptic-0.51-alt-rpmgroups-intl.patch
Patch4: synaptic-0.56-alt-build-fix.diff
Patch5: synaptic-0.57.2-gcc4-fix.patch
Patch6: synaptic-0.57.2-gcc43-fix.patch

Requires: %{get_dep rpm}, %{get_dep libapt}

BuildRequires(pre): libapt-devel
# Original BuildPreReq was: libapt-devel >= 0.5.4cnc9
# To build synaptic with apt < 0.5.5cnc5 apply Patch1
BuildPreReq: libapt-devel >= 0.5.5cnc5
%if_enabled autotools
BuildPreReq: intltool
%endif

# From configure.in
BuildPreReq: gcc-c++ xmlto librpm-devel libpopt-devel libapt-devel
BuildPreReq: libgtk+2-devel >= 2.4.0
BuildPreReq: libglade2-devel >= 2.0.0
BuildPreReq: libvte-devel >= 0.10.11

BuildPreReq: libtinfo-devel scrollkeeper

%description
Synaptic is a graphical front-end for APT (Advanced Package Tool).
It attempts to be a lot easier to use than other existing APT front-ends.

Instead of using trees to display packages, Synaptic is heavily based on a
powerful package filtering system. That greatly simplifies the interface
while giving a lot more flexibility to browse through very long package
lists.

%description -l ru_RU.CP1251
Synaptic - ��� ����������� �������� ��� APT (Advanced Package Tool).
��� ����������� ����� � �������������, ��� ������ �������� ��� APT.

������ ������������� ������ ��� ����������� ������� synaptic �������
�� ������ ������� ���������� �������. ��� ����������� �������� ���������
� ������ � ��� ������������� ������� ������ �������� ��� ��������� ��
����� ������� ������� �������.

%prep
%setup -q

# See comments about this patch above
#%%patch1 -p1
#%%patch2 -p1
#%%patch3 -p1
%patch4 -p1
%patch5
%patch6 -p1

%if_with ru_po
# installing own translation
install -p -m644 %SOURCE1 po/ru.po
# remove gmo file to tell autotools recreate it
rm -fv -- po/ru.gmo
%endif

install -p -m644 %SOURCE3 pixmaps/package-supported.png

%build
intltoolize --force
%if_enabled autotools
aclocal
autoheader
automake -a -c
autoconf
%endif

%add_optflags -fno-exceptions
%configure --with-vte --with-pkg-hold --enable-scripts
%make_build

%install
%makeinstall_std

mkdir -p %buildroot%_mandir/ru/man8/
%if_with ru_man
install -p -m644 %SOURCE2 %buildroot%_mandir/ru/man8/%name.8
%else
install -p -m644 man/%name.ru.8 %buildroot%_mandir/ru/man8/%name.8
%endif

mkdir -p %buildroot%_sysconfdir/apt/apt.conf.d
install -p -m644 %SOURCE4 %buildroot%_sysconfdir/apt/apt.conf.d/%name.conf

%find_lang %name

%files -f %name.lang
%_sbindir/*
%_datadir/%name
%_datadir/gnome/help/%name
%_datadir/omf/%name
%_man8dir/%name.8.*
%_mandir/ru/man8/%name.8.*
%config(noreplace) %_sysconfdir/apt/apt.conf.d/%name.conf
%doc README* TODO NEWS AUTHORS

%exclude %_desktopdir/%{name}*.desktop
%exclude %_datadir/pixmaps/%name.png

%changelog
* Fri Jan 21 2011 Lenar Shakirov <snejok@altlinux.ru> 0.57.3-alt1
- New version
- Spec cleaned: thanks to rpmcs!
- %_sysconfdir/X11/sysconfig/%name.desktop dropped

* Tue Nov 09 2010 Lenar Shakirov <snejok@altlinux.ru> 0.57.2-alt4
- gcc 4.5 related fixes: synaptic-0.57.2-gcc4-fix.patch updated

* Mon Nov 01 2010 Lenar Shakirov <snejok@altlinux.ru> 0.57.2-alt3
- Supported labels: sisyphus fixed, additional labels added (closes: #20538)

* Mon Dec 21 2009 Alexey I. Froloff <raorn@altlinux.org> 0.57.2-alt2.7.1
- NMU:
  + rebuilt with apt 0.5.15lorg2-alt31.1

* Fri Oct 16 2009 Slava Semushin <php-coder@altlinux.ru> 0.57.2-alt2.7
- Rephrase message translation (Closes: #21948)

* Tue Jul 21 2009 Slava Semushin <php-coder@altlinux.ru> 0.57.2-alt2.5.M40.1
- Backported to 4.0 branch (Closes: #12690)

* Tue Jul 21 2009 Slava Semushin <php-coder@altlinux.ru> 0.57.2-alt2.5.M41.1
- Backported to 4.1 branch

* Tue Jul 21 2009 Slava Semushin <php-coder@altlinux.ru> 0.57.2-alt2.5.M50.1
- Backported to 5.0 branch

* Sun Jul 19 2009 Slava Semushin <php-coder@altlinux.ru> 0.57.2-alt2.6
- NMU
- Updated Russian translation (Alexandre Prokoudine, Closes: #20802, #20259)

* Sat Jan 10 2009 Slava Semushin <php-coder@altlinux.ru> 0.57.2-alt2.5
- NMU: fixed typo in translation (Closes: #15137)

* Fri Dec 12 2008 Fr. Br. George <george@altlinux.ru> 0.57.2-alt2.4
- Fix GCC4.3.x build

* Wed Jul 18 2007 Anton V. Boyarshinov <boyarsh@altlinux.ru> 0.57.2-alt2.3
- Ported the fix for 'Pin package' segfault from upstream. (avm@)

* Sun Dec 17 2006 Michael Shigorin <mike@altlinux.org> 0.57.2-alt2.2
- NMU: applied ru.po patch by Vitaly Lipatov (lav@); fixes #4533
- spec macro abuse cleanup

* Wed Oct 18 2006 Michael Shigorin <mike@altlinux.org> 0.57.2-alt2.1
- NMU: rebuilt against current libvte

* Fri Jun 16 2006 Alexey Rusakov <ktirf@altlinux.ru> 0.57.2-alt2
- fixed building with GCC4.
- squeezed buildreqs.
- added scrollkeeper-related stuff to post-scripts.

* Mon Apr 03 2006 ALT QA Team Robot <qa-robot@altlinux.org> 0.57.2-alt1.1
- Rebuilt with libapt-0.5.15lorg2.

* Sat Aug 27 2005 Sviatoslav Sviridov <svd@altlinux.ru> 0.57.2-alt1
- Updated to 0.57.2
- Updated russian translation
- Added synaptic.conf in %_sysconfdir/apt/apt.conf.d/
  + Enabled "mark-unsupported" option
- https://bugzilla.altlinux.org/images/favicon.ico used to mark
  supported packages

* Thu Feb 03 2005 Sviatoslav Sviridov <svd@altlinux.ru> 0.56-alt0.3
- Updated russian translation (thanx to Vitaly Lipatov)

* Wed Feb 02 2005 Sviatoslav Sviridov <svd@altlinux.ru> 0.56-alt0.2
- Updated to 0.56pre2
- Fixed build dependencies
- Build with VTE support instead of ZVT

* Wed Jan 19 2005 ALT QA Team Robot <qa-robot@altlinux.org> 0.55.3-alt1.1
- Rebuilt with libstdc++.so.6.

* Tue Nov 09 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.55.3-alt1
- updated to 0.55.3
- updated russian translation (thanks to Vitaly Lipatov)

* Tue Nov 02 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.55-alt1
- Updated to 0.55

* Wed Oct 20 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.53.4-alt2
- applied patch synaptic-fedora-exit-on-cancel-fix.diff
  (fixes eventual segfaults when selecting packages)

* Fri Sep 24 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.53.4-alt1
- 0.53.4

* Wed Aug 18 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.53-alt1
- 0.53

* Sun Jul 11 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.48.2-alt3
- Fixed segfault when system has broken packages (#4359)
- updated russian translation

* Sun Jun 27 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.48.2-alt2
- created patch to perform group names translation using RPM messages
  (#4360)
- fixed typo in translation

* Thu May 13 2004 ALT QA Team Robot <qa-robot@altlinux.org> 0.48.2-alt1.1
- Rebuilt with libapt-0.5.15cnc6-alt1.

* Mon Apr 12 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.48.2-alt1
- 0.48.2
- updated russian translation

* Mon Mar 15 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.48.1-alt1
- 0.48.1
- updated russian translation

* Wed Mar 10 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.48-alt0.2
- removed build dependency on libdb4.2-devel

* Wed Mar 10 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.48-alt0.1
- 0.48test2
- updated russian translation from kate@

* Sun Jan 18 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.47-alt2.2
- disabled synaptic-0.36-alt-state.patch

* Thu Jan 15 2004 Dmitry V. Levin <ldv@altlinux.org> 0.47-alt2.1
- Rebuilt with apt-0.5.15cnc5.

* Tue Jan 13 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.47-alt2
- russian manpage moved to %_mandir/ru/man8 (#3470)
- spec update:
  + conditional using of original russian manpage or included in src.rpm

* Fri Jan 02 2004 Sviatoslav Sviridov <svd@altlinux.ru> 0.47-alt1
- updated russian translation from kate@

* Mon Dec 15 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.47-alt0.2
- build html docs using xsltproc instead of jade (thanks to Vitaly Ostanin)
- updated BuildRequires
- spec update: conditional using of autotools

* Sun Dec 14 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.47-alt0.1
- 0.47
- russian translation from kate@
- spec supports conditional build with own ru.po
- updated BuildRequires

* Thu Oct 23 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.45-alt1
- 0.45
 + proxy can be configured with the gui now
 + new preferences dialog layout (thanks Sebastian Heinlein)
 + new filter dialog layout  (thanks Sebastian Heinlein)
 + when adding filter, they are called "New Filter 1", "New Filter 2"
    etc
 + added "search lack", so that the interactive search does not start
   immediately
 + new quit dialog
 + speedup for the pattern filters
 + small ui-enhancements in the filter dialog
 + basic support for distribution selection (if you have more than one
   distro in your sources.list, you can choose which to use by
   default)
 + new about dialog (thanks Sebastian)
 + support to choose between different versions of a given package
 + updated translations

* Wed Sep 24 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.44-alt1
- 0.44:
  + two alternative main layouts can be choosen now
  + "clean cache now" implemented" in preferences window
  + the search entry in the main window is interactive again
  + much improved preferences dialog (thanks to Sebastian Heinlein)
  + new italian translation (thanks  Mauro Colorio and  Luigi Maselli)
  + updated spanish translation (thanks Fco. Javier Fernandez)
  + updated russian translation (thanks Sviatoslav Sviridov)
  + new belarussian translation (thanks Vital Khilko)
  + updated chinese translation (thanks liu jack)

* Thu Aug 21 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.43.1-alt1
- new in 0.43.1:
  + the descriptionParser for rpm has changed
    (the actuall parser is configurable now)
  + new find and changes dialog layout, more HIG compliant
    (thanks to Sebastian Heinlein)
  + more bugfixes in the PkgTreeView and the new sorting code
- new in 0.42:
  + download percentage is now always calculated correctly
    (thanks to David Wilson)
  + save selection on "pin", "update" and "proceed" now
  + some bugs fixed in the new sortable tree/list
  + the order of the columns in the treeview is configurable now
  + updated german translation (thanks to Sebastian Heinlein)
  + updated spanish translation (thanks to Javier Serrador)
  + updated japanese translation (thanks to Daisuke SUZUKI)
  + updated russian translation (thanks to Sviatoslav Sviridov)
  + [debian only] debian package tags support
  + sortable installed size column added
  + "Information" tab now scrollable
- updated russian translation
- added belarussian translation (thanks to Vital Khilko)

* Sat Aug 02 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.40-alt1
- 0.40
  + updated japanese translation
  + new status "Obsolete and locally installed" for status view
  + added depends and provides to the "Find tool" window
  + more predefined filters (new, residual config, debconf)
  + added "provides" tab
  + added "remove with dependencies" feature
  + default action for delete button can be configured
- updated russian translation

* Fri Jun 27 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.38-alt1
- 0.38
- updated russian translation

* Thu Jun 19 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.37-alt1
- 0.37
- updated russian translation

* Mon Apr 28 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.36.1-alt1
- 0.36.1
- disabled pkgDepCache::State support until apt-0.5.5cnc5 in Sisyphus

* Tue Apr 01 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.35.1-alt1
- 0.35.1

* Mon Mar 31 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.35-alt1
- 0.35

* Mon Mar 10 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.35-alt0.2
- SVN snapshot 2003-03-10

* Wed Feb 26 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.35-alt0.1
- SVN snapshot

* Tue Jan 28 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.32-alt2
- fixed work with buffer in rgsummarywindow.cc

* Mon Jan 27 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.32-alt1
- 0.32
  + bugfixes
  + translation updates
  + toolbar buttons can be configured now
- Updated BuildRequires

* Thu Jan 16 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.31-alt2
- Updated russian translation
- Fixed bug #1969:
  + Don't exit if failed to read cache files

* Tue Jan 14 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.31-alt1
- 0.31
- updated russian translation (thanks to Vitaly Lipatov)
- fixed segfault in cache operations

* Tue Jan 07 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.30-alt2
- build with libzvt support
- patch for using RGZvtInstallProgress
- updated BuildRequires
- updated russian translation

* Sat Jan 04 2003 Sviatoslav Sviridov <svd@altlinux.ru> 0.30-alt1
- 0.30
- utf8 patch from Anthony Fok <anthony@thizlinux.com>
- updated BuildRequires
- updated %%files section

* Thu Dec 05 2002 Dmitry V. Levin <ldv@altlinux.org> 0.28.1-alt2
- Removed all usermode stuff from this package.
- Fixed i18n support.

* Mon Dec 02 2002 Sviatoslav Sviridov <svd@altlinux.ru> 0.28.1-alt1
- 0.28.1
- applied patches for build with latest automake/autoconf tools

* Mon Nov 25 2002 Sviatoslav Sviridov <svd@altlinux.ru> 0.27-alt2
- fixed segfault in filter editor

* Thu Nov 21 2002 Sviatoslav Sviridov <svd@altlinux.ru> 0.27-alt1
- 0.27
- configure.in patch: use -ldb instead of -ldb-3.1 for find rpm
- requild for new apt-0.5 relase: 0.5.4cnc9
- updated BuildRequires
- added 'genericname' to menu file

* Wed Oct 16 2002 Sviatoslav Sviridov <svd@altlinux.ru> 0.25-alt1
- 0.25
- patch for gcc3.2

* Thu Oct 10 2002 AEN <aen@altlinux.ru> 0.16-alt4
- rebuilt with gcc3.2
- patch from Aleksey Dyachenko for gcc3.2

* Sat Aug 10 2002 Alexey Voinov <vns@altlinux.ru> 0.16-alt3
- rebuild with patched WINGs (fix bug #521)

* Wed Mar 27 2002 Dmitry V. Levin <ldv@alt-linux.org> 0.16-alt2
- Added librpm-4.0.4 build support.
- Built with librpm-4.0.4, updated buildrequires.

* Fri Nov 16 2001 Alexander Bokovoy <ab@altlinux.ru> 0.16-alt1
- 0.16 (nothing new, prepared for APT 0.5)

* Tue Nov 06 2001 Dmitry V. Levin <ldv@alt-linux.org> 0.15-alt7
- Initial build with rpm4.
- Moved usermode stuff to separate package.

* Thu Oct 11 2001 AEN <aen@logic.ru> 0.15-alt6
- rebuilt with libpng.so.3

* Thu Aug 09 2001 Dmitry V. Levin <ldv@altlinux.ru> 0.15-alt5
- Corrected requires and buildrequires lists.
- Reworked compilation options again: we add only '-fno-exceptions' now.

* Wed Aug 08 2001 Dmitry V. Levin <ldv@altlinux.ru> 0.15-alt4
- Added %optflags_nocpp to compilation options.
- Rebuilt with apt-0.3.19cnc52-alt2, corrected apt requires.

* Sun Aug 05 2001 Alexander Bokovoy <ab@altlinux.ru>   0.15-alt3
+ Fixed:
- wmsetbg >= 0.66 is required for proper function
- BuildRequires and Requires are refined

* Wed Jul 25 2001 Alexander Bokovoy <ab@altlinux.ru>   0.15-alt2
+ Fixed:
- group tag

* Tue Jul 24 2001 Alexander Bokovoy <ab@altlinux.ru>   0.15-alt1
+ synaptic-0.15
- Russian menus
- Requirements are refined for installation and build
- Userhelper support for Synaptic

* Tue Apr 10 2001 Alexander Bokovoy <ab@avilink.net>	0.8-alt1
+ Russian translation added
- First build for ALTLinux

* Wed Feb 14 2001 Alfredo K. Kojima <kojima@conectiva.com.br>
+ synaptic-0.8-1cl
- first official release (closes: #1417)

* Wed Jan 24 2001 Alfredo K. Kojima <kojima@conectiva.com.br>
+ synaptic-0.7-1cl
- i18n
- pt_BR

* Wed Jan 24 2001 Alfredo K. Kojima <kojima@conectiva.com.br>
+ synaptic-0.6-1cl
- depends on apt cnc32

* Thu Jan 23 2001 Alfredo K. Kojima <kojima@conectiva.com.br>
+ synaptic-0.5-1cl
- renamed from raptor to Synaptic

* Mon Jan 22 2001 Alfredo K. Kojima <kojima@conectiva.com.br>
+ raptor-0.4-1cl

* Tue Jan 18 2001 Alfredo K. Kojima <kojima@conectiva.com.br>
+ raptor-0.3-1cl

* Mon Jan 15 2001 Alfredo K. Kojima <kojima@conectiva.com.br>
+ raptor-0.2-1cl
- release version 0.2 (first)

