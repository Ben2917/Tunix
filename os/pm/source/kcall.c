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
* This module manages system requests to the kernel.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/fcntl.h"
#include    "../include/signal.h"
#include    "../include/proc.h"
#include    "../include/comm.h" 
#include    "../include/ipc.h"
#include    "../include/msg.h"
#include    "../include/kmsg.h"
#include    "../include/fcodes.h"
#include    "../include/pid.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/kcall.h"

extern int  errno;
extern int  fdl;
extern int  msg_k;
extern int  msg_u;

extern int  write();

static MSG  msg;
 
static int  msgsz = sizeof(MSG) - sizeof(long);

static void ker_request();

/*---------------------------------------------------*/

void panic(s)

char *s;

{
    msg.cmd   = PANIC;

    (void) STRCPY(msg.str1, s); 
 
    ker_request();
}

/*---------------------------------------------------*/

void ker_exit(pid, new_pid, sig)

INT     pid;
int     new_pid;
INT     sig;

{
    msg.cmd   = EXIT;
    msg.arg1  = (ARGTYPE) new_pid;
    msg.arg2  = (ARGTYPE) sig;
    msg.arg3  = (ARGTYPE) pid;

    ker_answer(&msg, FALSE);
}

/*---------------------------------------------------*/

static void ker_request()

{
    msg.mtype = (long) PM_PID;
    msg.queue = 0;
    msg.class = KER_MSG;
    msg.pid   = PM_PID;

    (void) msgsnd(msg_k, (struct msgbuf *)&msg, msgsz, 0);

    if ( fdl != -1 )
        (void) write(fdl, (char *)&msg, sizeof(MSG));

    (void) msgrcv(msg_u, (struct msgbuf *)&msg, msgsz, (long) PM_PID, 0);

    if ( fdl != -1 )
        (void) write(fdl, (char *)&msg, sizeof(MSG));
}

/*---------------------------------------------------*/

void ker_answer(m, sup)

MSG     *m;
int     sup;

{
    m->mtype = (long) PM_PID;
    m->queue = 0;
    m->pid   = PM_PID;

    (void) msgsnd(msg_k, (struct msgbuf *)m, msgsz, 0);

    if ( fdl != -1 )
        (void) write(fdl, (char *)m, sizeof(MSG));

    if ( sup )
        suppress();
}

/*---------------------------------------------------*/

/* this function is called in proc.c in cases PAUSE, WAIT   */

void notify(pid, error, iarg, Iarg)

INT     pid;
int     error;
int     iarg;
INT     Iarg;

{
    msg.cmd   = REPLY;
    msg.errno = error;
    msg.arg1  = (ARGTYPE) iarg; 
    msg.arg2  = (ARGTYPE) Iarg;

    send(pid, &msg, UNCRITICAL);
}

/*---------------------------------------------------*/

void call_handler(pid, sig)

INT     pid;
INT     sig;

{
    msg.cmd  = CALL_HDLR;
    msg.arg1 = (ARGTYPE) pid;
    msg.arg2 = (ARGTYPE) sig;

    ker_answer(&msg, FALSE);
}

