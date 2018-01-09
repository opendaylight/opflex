/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Utility functions for command-line programs
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

namespace opflexagent {

void daemonize()
{
    pid_t pid, sid;
    int fd;

    if ( getppid() == 1 ) return;
    pid = fork();
    if (pid < 0)
        exit(1);

    if (pid > 0)
        exit(EXIT_SUCCESS);
    sid = setsid();
    if (sid < 0)
        exit(1);
    if ((chdir("/")) < 0)
        exit(1);

    fd = open("/dev/null",O_RDWR, 0);

    if (fd != -1) {
        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);
        if (fd > 2)
            close (fd);
    }
    umask(027);
}

} /* namespace opflexagent */
