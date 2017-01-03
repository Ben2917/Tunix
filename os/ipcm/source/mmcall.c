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

ADDR mm_shmat(pid, rno, pde, addr, size, flg)

INT     pid;
int     rno;
INT     pde;
ADDR    addr;
INT     size;
int     flg;

{
    ADDR    result; 
    int     err;
    INT     chan = chan_fetch((char *)&result, (char *)&err);

    msg.cmd  = SHMAT;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) pid;
    msg.arg2 = (ARGTYPE) rno;
    msg.arg3 = (ARGTYPE) addr;
    msg.arg4 = (ARGTYPE) size;
    msg.arg5 = (ARGTYPE) pde;
    msg.arg6 = (ARGTYPE) flg;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return result;
}

/*---------------------------------------------------------*/

int mm_shmdt(pid, addr)

INT     pid;
ADDR    addr;

{
    int rno; 
    int err;
    INT chan = chan_fetch((char *)&rno, (char *)&err);

    msg.cmd  = SHMDT;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) pid;
    msg.arg2 = (ARGTYPE) addr;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return rno;
}

/*---------------------------------------------------------*/

int mm_alloc(type)

SHORT   type;

{
    int rno; 
    int err;
    INT chan = chan_fetch((char *)&rno, (char *)&err);

    msg.cmd  = ALLOC;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) 0;
    msg.arg2 = (ARGTYPE) type;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return rno;
}

/*---------------------------------------------------------*/

int mm_free(rno)

int     rno;

{
    int res;
    int err;
    INT chan = chan_fetch((char *)&res, (char *)&err);

    msg.cmd  = FREE;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) rno;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return res;
}

/*---------------------------------------------------------*/

INT mm_grow(rno, size)

int     rno;
INT     size;

{
    INT pde; 
    int err;
    INT chan = chan_fetch((char *)&pde, (char *)&err);

    msg.cmd  = GROW;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) 0;
    msg.arg3 = (ARGTYPE) (1 + (size - 1) / PAGE_SIZE);
    msg.arg4 = (ARGTYPE) 0;

    send(MM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return pde;
}

/*---------------------------------------------------------*/

void mm_done(chan, res, error)

INT chan;
int res;
int error;

{
    int *rp;
    int *ep;

    chan_pointers(chan, &rp, &ep);

    *rp = res;
    *ep = error;

    chan_wakeup(chan);
}

/*---------------------------------------------------------*/

void mm_adone(chan, adr, error)

INT     chan;
ADDR    adr;
int     error;

{
    ADDR    *ap;
    int     *ep;

    chan_pointers(chan, &ap, &ep);

    *ap = adr;
    *ep = error;

    chan_wakeup(chan);
}

