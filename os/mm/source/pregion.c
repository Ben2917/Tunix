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
* This module manages the per process region tables (pregions)
* for the set of all processes in the system.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/region.h"
#include    "../include/errno.h"
#include    "../include/ahdr.h"
#include    "../include/pid.h"
#include    "../include/fmcall.h"
#include    "../include/kcall.h"
#include    "../include/pregion.h"
#include    "../include/macros.h"

extern int errno;

#define N_PROCS     128         /* max. no of active processes          */ 
#define MAX_REGS    20          /* max. number of regions per proc.     */ 
#define MAX_SIZE    32 * 1024   /* max. number of pages per process     */
 
typedef struct
{
    ADDR    base;           /* base virtual address of region   */
    INT     size;           /* size of region in pages          */
    int     rno;            /* logical region number            */
    INT     pde;            /* page directory entry for region  */
    SHORT   type;           /* see region.h                     */

} PREGION;

typedef struct
{
    INT     pid;            /* identifier of process            */
    LONG    fid;            /* descriptor of executable binary  */
    INT     size;           /* size in pages                    */
    INT     pd;             /* frame nr. of page directory      */
    PREGION pregs[MAX_REGS];/* regions belonging to this proc.  */

} PROC;

static PROC     proc_table[N_PROCS];

#define ERROR(eno)      { errno = eno; return -1; }

/*-------------- Declarations of local functions -------------------*/

static int  prg_alloc();
static void prg_grow();
static void prg_free();
static PROC *which_proc();
static PROC *new_proc();

/*-------------- Definitions of public functions -------------------*/

void prg_init()

{
    INT         i;
    PROC        *pp;

    for ( i = 0, pp = proc_table; i < N_PROCS; ++i, ++pp )
        pp->pid = -1;

    pp = proc_table;
    pp->pid = INIT_PID;
    pp->pd  = pd_alloc(INIT_PID);

    reg_init();
}

/*-------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: prg_fork                                       */
/*                                                          */
/* Entry conditions:                                        */
/*    old_pid, new_pid are process identifiers for the      */
/*    parent and child processes.                           */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The text region is attached to the child process,     */
/*    since it is shared.                                   */
/* 2) The data region is duplicated and attached to the     */
/*    child process.                                        */
/* 3) Any other regions belonging to the parent are         */
/*    simply attached to the child, since they are pre-     */
/*    sumably shared memory.                                */
/* 4) Upon return the appropriate page directory entries    */
/*    have already been made in the appropriate directory.  */
/*                                                          */
/************************************************************/

void prg_fork(new_pid, old_pid)

INT     new_pid;
INT     old_pid; 

{
    PROC    *op   = which_proc(old_pid);
    PROC    *np   = new_proc(new_pid);
    PREGION *oprp = op->pregs;
    PREGION *nprp = np->pregs;
    INT     i;
   
    np->pd = pd_alloc(new_pid);

    (void) MEMCPY(nprp, oprp, sizeof(PREGION));
    reg_attach(nprp->rno);      /* share text region    */
    pde_add(np->pd, nprp->base, nprp->pde);
    
    ++oprp;
    ++nprp;

    (void) MEMCPY(nprp, oprp, sizeof(PREGION));
    nprp->rno = reg_dup(oprp->rno, &nprp->pde); /* duplicate data region */
    pde_add(np->pd, nprp->base, nprp->pde);

    ++oprp;
    ++nprp;

    if ( oprp->rno == -1 )
        nprp->rno = -1;
    else
    {
        (void) MEMCPY(nprp, oprp, sizeof(PREGION));
        nprp->rno = reg_dup(oprp->rno, &nprp->pde); /* duplicate bss region  */
        pde_add(np->pd, nprp->base, nprp->pde);
    }

    for ( i = 3, ++oprp, ++nprp; i < MAX_REGS; ++i, ++oprp, ++nprp )
    {
        if ( oprp->rno == -1 )
        {
            nprp->rno = -1;
            continue;
        }

        (void) MEMCPY(nprp, oprp, sizeof(PREGION)); 
        reg_attach(nprp->rno);
        pde_add(np->pd, nprp->base, nprp->pde);
    }
}

/*-------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: prg_exec                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pid is the process identifier for this process.       */
/* 2) path identifies the exectuable binary from which the  */
/*    text and data for the process are to be taken.        */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The regions previously belonging to the process are   */
/*    released.                                             */
/* 2) New regions of the appropriate sizes and types are    */
/*    created for the text, data and blank static storeage. */
/* 3) The return value is the total size of the process     */
/*    in pages if everything went well. If some region      */
/*    couldn't be allocated, the return value is -1.        */
/*                                                          */
/************************************************************/

int prg_exec(pid, path)

INT     pid;
char    *path;

{
    PROC    *pp = which_proc(pid); 
    AHDR    ahdr;
    int     prno, i;
    PREGION *prp = pp->pregs;
    LONG    fid;

    if ( fm_exec(path, pid, &fid) == -1 )
        return -1;

    pp->fid = fid;

    if ( fm_ahdr(pid, &ahdr) == -1 )
        return -1;

    pp->size = ahdr.size_text + ahdr.size_data + ahdr.size_bss; 
 
    if ( pp->size > MAX_SIZE )
        return -1;

    /* release all regions held up to now   */

    for ( i = 0; i < MAX_REGS; ++i )
        if ( prp[i].rno != -1 )
            prg_free(pp, (INT) i);

    if ( (prno = prg_alloc(pp, fid, R_TEXT)) == -1 )
        return -1;

    prg_grow(pp, (INT)prno, ahdr.vadr_text, ahdr.size_text, ahdr.fblk_text);
    pde_add(pp->pd, ahdr.vadr_text, prp[prno].pde);
 
    if ( (prno = prg_alloc(pp, fid, R_DATA_W)) == -1 )
    {
        prg_free(pp, 0);
        return -1;
    }

    prg_grow(pp, (INT)prno, ahdr.vadr_data, ahdr.size_data, ahdr.fblk_data);
    pde_add(pp->pd, ahdr.vadr_data, prp[prno].pde);

    if ( ahdr.size_bss )
    {
        if ( (prno = prg_alloc(pp, fid, R_BSS)) == -1 )
        {
            prg_free(pp, 0);
            prg_free(pp, 1);
            return -1;
        }

        prg_grow(pp, (INT)prno, ahdr.vadr_bss, ahdr.size_bss, (BLKNO)0);
        pde_add(pp->pd, ahdr.vadr_bss, prp[prno].pde);
    }

    return 0;
}

/*-------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: prg_exit                                       */
/*                                                          */
/* Entry conditions:                                        */
/*    pid is the process identifier of the exiting process. */
/*                                                          */
/* Exit conditions:                                         */
/*    All regions attached to the process have been de-     */ 
/*    tached and freed.                                     */
/*                                                          */
/************************************************************/

void    prg_exit(pid)

INT     pid;

{
    PROC    *pp = which_proc(pid);
    INT     i;

    for ( i = 0; i < MAX_REGS; ++i )
        if ( pp->pregs[i].rno != -1 )
            prg_free(pp, i);

    pp->pid = -1;
}

/*-------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: prg_brk                                        */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pid is the identifier of a process.                   */
/* 2) new_end_data is the new virtual address at which the  */
/*    data region is to end.                                */
/* 3) max_inc is the maximum increment, by which the data   */
/*    region will be allowed to grow without exceding the   */
/*    system limit on the size of a process.                */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The return value is -1, if new_end_data is not in     */
/*    the allowable range (and errno set appropriately).    */
/* 2) Otherwise the data region has expanded (or shrunk)    */
/*    as appropriate and the return value is 0.             */
/*                                                          */
/************************************************************/

int prg_brk(pid, new_end_data, max_inc)

INT     pid;
ADDR    new_end_data;
INT     max_inc;   

{
    PROC    *pp  = which_proc(pid);
    PREGION *prp = &(pp->pregs[2]);
    ADDR    edata = prp->base + PAGE_SIZE * prp->size;
    INT     new_size;

    if ( new_end_data > edata && (edata + max_inc < new_end_data) )
        ERROR(ENOMEM)

    if ( new_end_data < prp->base )
        ERROR(EINVAL)

    new_size = (INT) ((new_end_data - prp->base) / PAGE_SIZE);

    prg_grow(pp, 2, prp->base, new_size, (BLKNO)0);
    prp->size = new_size;

    return 0;
}

/*-------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: prg_shmat                                      */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pid is the identifier of a process.                   */
/* 2) rno is a legal region number (gotten from mm).        */
/* 3) base is the virtual address at which the shared       */
/*    data region is to begin.                              */
/* 4) size is the size in pages this shared region is to    */
/*    have for process pno.                                 */
/* 5) pde is the page directory entry for this region.      */
/* 6) rdonly is TRUE if the region is to be read only.      */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The return value is -1, if no more regions are avail- */
/*    able or addresses overlap (errno set appropriately).  */
/* 2) Otherwise the shared region has been attached to pno  */
/*    and the return value is 0.                            */
/*                                                          */
/************************************************************/

ADDR prg_shmat(pid, rno, base, size, pde, rdonly)

INT     pid;
int     rno;
ADDR    base;
INT     size;   
INT     pde;
int     rdonly;

{
    PROC    *pp = which_proc(pid);
    PREGION *prp, *prq;
    INT     i;
    INT     max_indx = 0;
    INT     rsize;

    rsize = 1 + (size - 1) / PAGE_SIZE;

    if ( pp->size + rsize > MAX_SIZE )
    {
        errno = ENOMEM;
        return (ADDR)0;
    }

    pp->size += rsize;

    for ( i = 0, prp = pp->pregs; i < MAX_REGS; ++i, ++prp )
        if ( prp->rno == -1 )
            break;

    if ( i >= MAX_REGS )
    {
        errno = EMFILE;
        return (ADDR)0;
    }

    if ( base == (ADDR)0 )
    {
        for ( i = 0, prq = pp->pregs; i < MAX_REGS; ++i, ++prq )
            if ( prq->rno != -1  && ((prq->base) >> 22) > max_indx )
                max_indx = (prq->base) >> 22;

        base = (max_indx + 1) << 22;
    }

    prp->base = base;
    prp->size = size;
    prp->rno  = rno;
    prp->pde  = pde;
    prp->type = ( rdonly ) ? R_DATA_R : R_DATA_W; 
    
    pde_add(pp->pd, base, pde);

    reg_attach(rno);

    return base;
}

/*-------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: prg_shmdt                                      */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pid is the identifier of a process.                   */
/* 2) addr is the base addr of the region to detach.        */
/*                                                          */
/* Exit conditions:                                         */
/* The shared region has been detached from process pno.    */
/* The return value is the region number of the detached    */
/* region.                                                  */
/*                                                          */
/************************************************************/

int prg_shmdt(pid, addr)

INT     pid;
ADDR    addr;

{
    PROC    *pp = which_proc(pid);
    PREGION *prp;
    INT     i;
    int     result;

    for ( i = 0, prp = pp->pregs; i < MAX_REGS; ++i, ++prp )
        if ( prp->base == addr )
            break;

    if ( i >= MAX_REGS )
        ERROR(EINVAL)

    reg_detach(prp->rno);

    result   = prp->rno;
    prp->rno = -1;

    return result;
}

/*-------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: prg_dump                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pid is the process identifier for the process.        */
/* 2) fd is the file descriptor (in the sense of the file   */
/*    manager) of the already opened core file.             */
/*                                                          */
/* Exit conditions:                                         */
/*    For each region belonging to the process the memory   */
/*    manager has been requested to send a copy of the      */
/*    region contents to the file manager to be put into    */
/*    the core file.                                        */
/*                                                          */
/************************************************************/

void prg_dump(pid, fd)

INT     pid;
int     fd;

{
    PROC    *pp = which_proc(pid);
    INT     i;
    PREGION *prp;

    for ( i = 0, prp = pp->pregs; i < MAX_REGS; ++i, ++prp )
        if ( prp->rno != -1 )
            (void) reg_dump(prp->rno, pid, fd);
}
              
/*-------------------------------------------------------------------*/

int prg_vfault(pid, addr, indx)

INT     pid;
ADDR    addr;
INT     indx;

{
    PROC        *pp = which_proc(pid); 
    PREGION     *prp;
    INT         prno;

    for ( prno = 0, prp = pp->pregs; prno < MAX_REGS; ++prno, ++prp )
        if ( prp->rno != -1 &&
             addr >= prp->base &&
             addr < prp->base + PAGE_SIZE * prp->size )
            break;

    if ( prno >= MAX_REGS )
        ERROR(EINVAL)

    return reg_vfault((INT) prp->rno, indx);
}

/*-------------------------------------------------------------------*/

int prg_pfault(pid, addr, indx)

INT     pid;
ADDR    addr;
INT     indx;

{
    PROC        *pp = which_proc(pid); 
    PREGION     *prp;
    INT         prno;

    for ( prno = 0, prp = pp->pregs; prno < MAX_REGS; ++prno, ++prp )
        if ( prp->rno != -1 &&
             addr >= prp->base &&
             addr < prp->base + PAGE_SIZE * prp->size )
            break;

    if ( prno >= MAX_REGS )
        ERROR(EINVAL)

    return reg_pfault((INT) prp->rno, indx);
}

/*-------------- Definitions of local functions ---------------------*/

static int prg_alloc(pp, fid, type)

PROC    *pp;
LONG    fid;
SHORT   type;

{
    int     i;
    PREGION *prp;

    for ( i = 0, prp = pp->pregs; i < MAX_REGS; ++i, ++prp )
        if ( prp->rno == -1 )
            break;

    if ( i >= MAX_REGS )
        ERROR(EMFILE)

    if ( type == R_TEXT )
        prp->rno = reg_xalloc(fid, type);
    else
        prp->rno = reg_alloc(fid, type);

    if ( prp->rno == -1 )
        return -1;

    reg_attach(prp->rno);

    prp->type = type;

    return i;
}

/*-------------------------------------------------------------------*/

static void prg_grow(pp, prno, base, size, fst_blk)

PROC    *pp;
INT     prno;
ADDR    base;
INT     size;
BLKNO   fst_blk;

{
    PREGION     *prp = &(pp->pregs[prno]);
    INT         indx, offset;

    adr2ind_off(base, &indx, &offset);
    prp->pde  = reg_grow(prp->rno, indx, size, fst_blk);
    prp->base = base;
    prp->size = size;
}

/*-------------------------------------------------------------------*/

static void prg_free(pp, prno)

PROC    *pp;
INT     prno;

{
    PREGION     *prp = &(pp->pregs[prno]);

    reg_detach(prp->rno);
    reg_free(prp->rno);

    prp->rno = -1;
}

/*-------------------------------------------------------------------*/

static PROC *which_proc(pid)

INT pid;

{
    int     pno;
    PROC    *pp;

    for ( pno = 0, pp = proc_table; pno < N_PROCS; ++pno, ++pp )
        if ( pp->pid == pid )
            return pp;

    return (PROC *)0;
}

/*-------------------------------------------------------------------*/

static PROC *new_proc(pid)

INT pid;

{
    int     pno, i;
    PROC    *pp;

    for ( pno = 0, pp = proc_table; pno < N_PROCS; ++pno, ++pp )
        if ( pp->pid == -1 )
        {
            pp->pid = pid;
            for ( i = 0; i < MAX_REGS; ++i )
                pp->pregs[i].rno = -1;

            return pp;
        }

    return (PROC *)0;
}


