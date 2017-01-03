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
* This module manages the system memory and associated structures.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/kutil.h" 
#include    "../include/macros.h"
#include    "../include/kmsg.h"
#include    "../include/task.h"
#include    "../include/pid.h"
#include    "../include/pte.h"
#include    "../include/fault.h"
#include    "../include/errno.h"

extern char *malloc();

extern int  errno;
 
#define NR_REGIONS  128

static MSG      msg;

static CHAR     memory[NR_PAGES * PAGE_SIZE];

static PTE      *ptables[NR_REGIONS];
static PTE      *pdirs[USR_TASKS];

static void     get_data_msg();
static void     put_data_msg();
static CHAR     *phys_addr();
static CHAR     *virt2phys();
static void     answer();

/*-------------------------------------------------------*/

/*
    The following definitions are only needed for emulation.
*/

#define READ    0
#define WRITE   1

#define NEW(typ, n)     (typ *) malloc((n) * sizeof(typ))

/*-------------------------------------------------------*/

int ptable(rno)

INT rno;

{
    msg.arg1 = (ARGTYPE) rno;

    answer(MM_PID);

    return rno;
}

/*---------------------------------------------------*/

void ptable_dup(old_rno, new_rno)

INT     old_rno;
INT     new_rno;

{
    if ( ptables[new_rno] == (PTE *)0 )
        ptables[new_rno] = NEW(PTE, 1024);

    (void) MEMCPY(ptables[new_rno], ptables[old_rno], 1024 * sizeof(PTE));
    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_build(rno, indx, flag)

INT     rno;
INT     indx;
SHORT   flag;

{
    PTE *pte;

    if ( ptables[rno] == (PTE *)0 )
        ptables[rno] = NEW(PTE, 1024);

    pte = ptables[rno] + indx;

    if ( flag )
        *pte = T_WRITE;
    else
        *pte = 0;

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_set_prsnt(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    PTE_PRESENT(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_set_notprsnt(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    NOT_PRESENT(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_write(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    PTE_WRITE(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_notwrite(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    NOT_WRITE(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void pte_is_prsnt(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    msg.arg1 = (ARGTYPE) PTE_IS_PRSNT(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_access(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    msg.arg1 = (ARGTYPE) PTE_ACCESS(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_dirty(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    msg.arg1 = (ARGTYPE) PTE_DIRTY(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_reset(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    PTE_RESET(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_set_frame(rno, indx, frm)

INT     rno;
INT     indx;
INT     frm;

{
    PTE *pte = ptables[rno] + indx;

    SET_FRAME(pte, frm);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void    pte_get_frame(rno, indx)

INT     rno;
INT     indx;

{
    PTE *pte = ptables[rno] + indx;

    msg.arg1 = (ARGTYPE) GET_FRAME(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void mem_copy(new_frm, old_frm)

INT new_frm;
INT old_frm;

{
    (void) MEMCPY(phys_addr(new_frm), phys_addr(old_frm), PAGE_SIZE);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void kmem_cpin(frm, chan)

INT     frm;
INT     chan;

{
    INT     done = 0;
    CHAR    *bp  = phys_addr(frm);

    for ( done = 0; done < PAGE_SIZE; done += DATA_SIZE, bp += DATA_SIZE )
        get_data_msg(MM_TASK, chan, bp);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void kmem_cpout(frm, chan)

INT     frm;
INT     chan;

{
    INT     done = 0;
    CHAR    *bp  = phys_addr(frm);

    for ( done = 0; done < PAGE_SIZE; done += DATA_SIZE, bp += DATA_SIZE )
        put_data_msg(FM_TASK, chan, bp);

    answer(MM_PID);
}

/*---------------------------------------------------*/

void mem_zero(frm)

INT frm;

{
    (void) MEMSET(phys_addr(frm), '\0', PAGE_SIZE);

    answer(MM_PID);
}

/*---------------------------------------------------*/

INT pd_alloc(tno)

INT     tno;

{
    if ( pdirs[tno] == (PTE *)0 )
        pdirs[tno] = NEW(PTE, 1024);

    msg.arg1 = (ARGTYPE) tno;

    answer(MM_PID);

    return tno;
}

/*---------------------------------------------------*/

void pde_build(pd, base, frm)

INT     pd;
ADDR    base;
INT     frm;

{
    PTE *pte;
    INT indx;

    indx = base >> 22;
    pte  = pdirs[pd] + indx;

    SET_FRAME(pte, frm);
    PTE_PRESENT(pte);

    answer(MM_PID);
}

/*---------------------------------------------------*/

INT adr2ind(addr)

ADDR    addr;

{
    return (INT) ((addr >> 12) & 0x3ff);
}

/*---------------------------------------------------*/

void adr2ind_off(addr)

ADDR    addr;

{
    msg.arg1 = (ARGTYPE) ((addr >> 12) & 0x3ff);
    msg.arg2 = (ARGTYPE) (addr & 0xfff);

    answer(MM_PID);
}

/*---------------------------------------------------*/

int mem_write(pd, addr, byte)

INT     pd;
ADDR    addr;
SHORT   byte;

{
    int     fault;
    CHAR    *pa = virt2phys(pd, addr, WRITE, &fault);

    if ( fault )
        return fault;

    *pa = (CHAR) byte;
    return 0;
}

/*---------------------------------------------------*/

int mem_read(pd, addr, bp)

INT     pd;
ADDR    addr;
SHORT   *bp;

{
    int     fault;
    CHAR    *pa = virt2phys(pd, addr, READ, &fault);

    if ( fault )
        return fault;

    *bp = (SHORT) *pa;
    return 0;
}

/*---------------------------------------------------*/

static CHAR *phys_addr(frm_nr)

INT     frm_nr;

{
    return memory + ((frm_nr - 256) << 12);
}

/*---------------------------------------------------*/

static CHAR *virt2phys(pd, addr, op, fault)

INT     pd;
ADDR    addr;
int     op;
int     *fault;

{
    INT     indx   = addr >> 22; 
    INT     offset = addr & 0xfff; 
    PTE     *pte   = pdirs[pd] + indx;
    INT     frm    = GET_FRAME(pte);

    indx = (addr >> 12) & 0x3ff;
    pte  = ptables[frm] + indx;

    *fault = 0;

    if ( !PTE_IS_PRSNT(pte) )
    {
        *fault = VALIDITY;
        return (CHAR *)0;
    }

    if ( op == WRITE )
    {
        if ( *pte & T_WRITE )
            *pte |= T_DIRTY;
        else
        {
            *fault = PROTECTION;
            return (CHAR *)0;
        }
    }

    *pte |= T_ACCESSED; 
 
    return (phys_addr(GET_FRAME(pte)) + offset);
}

/*---------------------------------------------------*/

static void get_data_msg(task, chan, buf)

INT     task;
INT     chan;
CHAR    *buf;

{
    DATA_MSG    *d = msg_d_next(task, chan);

    (void) MEMCPY(buf, d->data, DATA_SIZE);

    msg_d_free(d);
}

/*---------------------------------------------------*/

static void put_data_msg(task, chan, buf)

INT     task;
INT     chan;
CHAR    *buf;

{
    DATA_MSG    *d = msg_d_alloc();

    (void) MEMCPY(d->data, buf, DATA_SIZE);

    msg_d_append(task, chan, d); 
}

/*---------------------------------------------------*/

static void answer(queue)

INT     queue;

{
    msg.queue = queue;
    msg.cmd   = 0;
    send(&msg);
}


