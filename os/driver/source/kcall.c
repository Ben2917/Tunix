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
#include    "../include/comm.h" 
#include    "../include/kmsg.h"
#include    "../include/fcodes.h"
#include    "../include/pid.h"
#include    "../include/errno.h"
#include    "../include/macros.h"

extern int  errno;
extern int  msg_k;
extern int  msg_u;
extern int  my_pid;

static MSG  msg;
 
static int  msgsz = sizeof(MSG) - sizeof(long);

/*---------------------------------------------------*/

void panic(s)

char *s;

{
    msg.cmd   = PANIC;
    msg.mtype = (long) my_pid;
    msg.queue = 0;
    msg.class = KER_MSG;

    (void) STRCPY(msg.str1, s); 
 
    (void) msgsnd(msg_k, (struct msgbuf *)&msg, msgsz, 0);
    (void) msgrcv(msg_u, (struct msgbuf *)&msg, msgsz, (long) my_pid, 0);
}


