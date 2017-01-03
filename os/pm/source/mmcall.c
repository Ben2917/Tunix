/*
***********************************************************
*
* Module  : %M%
* Release : %I%
* Date    : %E%
*
* Copyright (c) 1989 R. M. Switzer
* 
* Permission to use, copy, or distribute this software for
* private or educational purposes is hereby granted without
* fee, provided that the above copyright notice and this
* permission notice appear in all copies of the software.
* The software may under no circumstances be sold or employed
* for commercial purposes.
*
* THIS SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF
* ANY KIND, EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT
* LIMITATION ANY WARRANTY OF FITNESS FOR A PARTICULAR PURPOSE. 
*
***********************************************************
*
* This module manages the communication with the memory
* manager.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/fcodes.h"
#include    "../include/kmsg.h"
#include    "../include/pid.h"
#include    "../include/comm.h"
#include    "../include/chan.h"
#include    "../include/mmcall.h"

extern int  errno;

static MSG  msg;

/*---------------------------------------------------------*/

int mm_fork(pid, ppid)

INT     pid;
INT     ppid;

{
    int res;
    int err;
    INT chan = chan_fetch((char *)&res, (char *)&err);

    msg.cmd  = FORK;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) pid;
    msg.arg2 = (ARGTYPE) ppid;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return res;
}

/*---------------------------------------------------------*/

int mm_exec(pid, path)

INT     pid;
char    *path;

{
    int size; 
    int err;
    INT chan = chan_fetch((char *)&size, (char *)&err);

    msg.cmd  = EXEC;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) pid;
    (void) STRCPY(msg.str1, path);

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return size;
}

/*---------------------------------------------------------*/

int mm_exit(pid)

INT     pid;

{
    int     res;
    int     err;
    INT     chan = chan_fetch((char *)&res, (char *)&err);

    msg.cmd  = EXIT;
    msg.chan = chan;
    msg.arg1 = (ARGTYPE) pid;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return res;
}
    
/*---------------------------------------------------------*/

int mm_brk(pid, new_end, max)

INT     pid;
ADDR    new_end;
INT     max;

{
    int res; 
    int err;
    INT chan = chan_fetch((char *)&res, (char *)&err);

    msg.cmd  = BRK;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) pid;
    msg.arg2 = (ARGTYPE) new_end;
    msg.arg3 = (ARGTYPE) max;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return res;
}

/*---------------------------------------------------------*/

void mm_dump(pid, fd)

INT     pid;
int     fd;

{
    int res;
    int err;
    INT chan = chan_fetch((char *)&res, (char *)&err);

    msg.cmd  = DUMP;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) pid;
    msg.arg2 = (ARGTYPE) fd;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
}

/*---------------------------------------------------------*/

void mm_done(chan, res, error)

INT chan;
int res;
int error;

{
    int     *rp;
    int     *ep;

    chan_pointers(chan, &rp, &ep);

    *rp = res;
    *ep = error;
    chan_wakeup(chan);
}


