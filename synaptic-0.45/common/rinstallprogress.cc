/* rinstallprogress.cc
 *
 * Copyright (c) 2000, 2001 Conectiva S/A
 *                     2002 Michael Vogt
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

#include <unistd.h>
#include <sys/fcntl.h>
#ifdef HAVE_RPM
#include <apt-pkg/configuration.h>
#endif

#include "rinstallprogress.h"

void *RInstallProgress::loop(void *data)
{
    RInstallProgress *me = (RInstallProgress*)data;

    me->startUpdate();
    while (me->_thread_id >= 0)
	me->updateInterface();
    me->finishUpdate();

    pthread_exit(NULL);
    
    return NULL;
}



pkgPackageManager::OrderResult RInstallProgress::start(pkgPackageManager *pm,
						       int numPackages)
{
    void *dummy;
    pkgPackageManager::OrderResult res;

    //cout << "RInstallProgress::start()" << endl;
 
#ifdef HAVE_RPM
    _config->Set("RPM::Interactive", "false");
    
    /*
     * This will make a pipe from where we can read child's output
     */

    // our stdout will be _stdout from now on, and our stderr will be _stderr
    _stdout = dup(1); 
    _stderr = dup(2);

    // create our comm. channel with the child
    int fd[2];
    pipe(fd);

    // make the write end of the pipe to the child become the new stdout 
    // and stderr (for the child)
    dup2(fd[1],1);
    dup2(1,2);

    close(fd[1]);

    // this is where we read stuff from the child
    _childin = fd[0];

    // make it nonblocking
    fcntl(_childin, F_SETFL, O_NONBLOCK);
#endif /* HAVE_RPM */

    _donePackages = 0;
    _numPackages = numPackages;

    // We must reset the _thread_id *before* calling pthread_create(),
    // otherwise the loop might test for its value before that function
    // returned.
    _thread_id = 0;

    _thread_id = pthread_create(&_thread, NULL, loop, this);

    res = pm->DoInstall();

    _thread_id = -1;
    pthread_join(_thread, &dummy);

#ifdef HAVE_RPM
    /*
     * Clean-up stuff so that everything is like before
     */
    close(_childin);
    dup2(_stdout, 1);
    dup2(_stderr, 2);
    close(_stdout);
    close(_stderr);
#endif /* HAVE_RPM */

    return res;
}

