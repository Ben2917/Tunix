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
* This module manages the communication with the IPC
* manager.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/chan.h"
#include    "../include/ahdr.h" 
#include    "../include/macros.h"
#include    "../include/fcodes.h" 
#include    "../include/kmsg.h"
#include    "../include/pid.h"
#include    "../include/comm.h"
#include    "../include/ipcmcall.h"

extern int  errno;

static MSG  msg;              

/*---------------------------------------------*/ 

int ipc_fork(pid, ppid)

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

    send(IM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);                     

    errno = err;
    return res;
}

/*---------------------------------------------*/

int ipc_exit(pid)

INT     pid;

{
    int res;
    int err;
    INT chan = chan_fetch((char *)&res, (char *)&err);

    msg.cmd  = EXIT;
    msg.chan = chan;
    msg.arg1 = (ARGTYPE) pid;
    send(IM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return res;
}

/*---------------------------------------------*/

int ipc_setuid(pid, uid, gid)

INT     pid;
SHORT   uid;
SHORT   gid;

{
    int res;
    int err;
    INT chan = chan_fetch((char *)&res, (char *)&err);

    msg.cmd  = SETEID;
    msg.chan = chan;
    msg.arg1 = (ARGTYPE) pid;
    msg.arg2 = (ARGTYPE) uid;
    msg.arg3 = (ARGTYPE) gid;

    send(IM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return res;
}

/*---------------------------------------------*/

void ipc_finish(chan, res, error)

INT     chan;  
int     res;
int     error;

{
    int *rp;
    int *ep;

    chan_pointers(chan, &rp, &ep);

    *rp = res;
    *ep = error;

    chan_wakeup(chan);
}
    

