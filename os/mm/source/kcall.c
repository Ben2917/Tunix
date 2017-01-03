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
#include    "../include/comm.h" 
#include    "../include/ipc.h"
#include    "../include/msg.h"
#include    "../include/kmsg.h"
#include    "../include/fcodes.h"
#include    "../include/pid.h"
#include    "../include/errno.h"
#include    "../include/macros.h"

extern int  errno;
extern int  msg_k;
extern int  msg_u;

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

/*-------------------------------------------------------*/

INT ptable(rno)

INT rno;

{
    msg.cmd  = PTABLE;
    msg.arg1 = (ARGTYPE) rno;

    ker_request();

    return (INT) msg.arg1;
}

/*---------------------------------------------------*/

void ptabl_dup(old_rno, new_rno)

INT     old_rno;
INT     new_rno;

{
    msg.cmd  = PTABLE_DUP;
    msg.arg1 = (ARGTYPE) old_rno;
    msg.arg2 = (ARGTYPE) new_rno;

    ker_request();
}

/*---------------------------------------------------*/

void    pte_build(rno, indx, flag)

INT     rno;
INT     indx;
SHORT   flag;

{
    msg.cmd  = PTE_BUILD;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;
    msg.arg3 = (ARGTYPE) flag;

    ker_request();
}

/*---------------------------------------------------*/

void    pte_set_prsnt(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_SET_PRSNT;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();
}

/*---------------------------------------------------*/

void    pte_set_notprsnt(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_SET_NOTPRSNT;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();
}

/*---------------------------------------------------*/

void    pte_write(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_WRITE;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();
}

/*---------------------------------------------------*/

void    pte_notwrite(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_NOTWRITE;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();
}

/*---------------------------------------------------*/

int     pte_is_prsnt(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_IS_PRSNT;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();

    return (int) msg.arg1;
}

/*---------------------------------------------------*/

int     pte_access(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_ACCESS;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();

    return (int) msg.arg1;
}

/*---------------------------------------------------*/

int     pte_dirty(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_DIRTY;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();

    return (int) msg.arg1;
}

/*---------------------------------------------------*/

void    pte_reset(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_RESET;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();
}

/*---------------------------------------------------*/

void    pte_set_frame(rno, indx, frm)

INT     rno;
INT     indx;
INT     frm;

{
    msg.cmd  = PTE_SET_FRAME;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;
    msg.arg3 = (ARGTYPE) frm;

    ker_request();
}

/*---------------------------------------------------*/

INT     pte_get_frame(rno, indx)

INT     rno;
INT     indx;

{
    msg.cmd  = PTE_GET_FRAME;
    msg.arg1 = (ARGTYPE) rno;
    msg.arg2 = (ARGTYPE) indx;

    ker_request();

    return (INT) msg.arg1;
}

/*---------------------------------------------------*/

void pde_add(pd, base, frm)

INT     pd;
ADDR    base;
INT     frm;

{
    msg.cmd  = PDE_BUILD;
    msg.arg1 = (ARGTYPE) pd;
    msg.arg2 = (ARGTYPE) frm;
    msg.arg3 = (ARGTYPE) base;

    ker_request();
}

/*---------------------------------------------------*/

INT pd_alloc(new_pid)

INT     new_pid;

{
    msg.cmd  = PD_ALLOC;
    msg.arg1 = (ARGTYPE) new_pid;

    ker_request();

    return (INT) msg.arg1;
}

/*---------------------------------------------------*/

void adr2ind_off(addr, ip, op)

ADDR    addr;
INT     *ip;
INT     *op;

{
    msg.cmd  = ADR2IND_OFF;
    msg.arg1 = (ARGTYPE) addr;

    ker_request();

    *ip = (INT) msg.arg1;
    *op = (INT) msg.arg2;
}
    
/*---------------------------------------------------*/

void mem_copy(new_frm, old_frm)

INT new_frm;
INT old_frm;

{
    msg.cmd  = MEM_COPY;
    msg.arg1 = (ARGTYPE) new_frm;
    msg.arg2 = (ARGTYPE) old_frm;

    ker_request();
}

/*---------------------------------------------------*/

void kmem_cpin(frm, chan)

INT     frm;
INT     chan;

{
    msg.cmd  = KMEM_CPIN;
    msg.arg1 = (ARGTYPE) frm;
    msg.chan = chan;

    ker_request();
}

/*---------------------------------------------------*/

void kmem_cpout(frm, chan)

INT     frm;
INT     chan;

{
    msg.cmd  = KMEM_CPOUT;
    msg.arg1 = (ARGTYPE) frm;
    msg.chan = chan;

    ker_request();
}

/*---------------------------------------------------*/

void mem_zero(frm)

INT frm;

{
    msg.cmd  = MEM_ZERO;
    msg.arg1 = (ARGTYPE) frm;

    ker_request();
}

/*---------------------------------------------------*/

static void ker_request()

{
    msg.mtype = (long) MM_PID;
    msg.queue = 0;
    msg.class = KER_MSG;
    msg.pid   = MM_PID;

    (void) msgsnd(msg_k, (struct msgbuf *)&msg, msgsz, 0);
    (void) msgrcv(msg_u, (struct msgbuf *)&msg, msgsz, (long) MM_PID, 0);
}
