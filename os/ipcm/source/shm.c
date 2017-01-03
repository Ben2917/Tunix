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
* This module implements the IPC shared memory.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/region.h"
#include    "../include/ipc.h"
#include    "../include/shm.h"
#include    "../include/ipcshm.h"
#include    "../include/id.h"
#include    "../include/proc.h"
#include    "../include/mmcall.h"
#include    "../include/comm.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/thread.h"

extern int      errno;

#define MAXSIZE     1024 * PAGE_SIZE

typedef struct
{
    IPC_PERM    perm;
    key_t       key;
    INT         segsz;      /* size of region           */
    int         rno;        /* region no of segment     */
    INT         pde;        /* page directory entry     */
    SHORT       lpid;       /* pid of last shmop        */
    SHORT       cpid;       /* pid of creator           */
    SHORT       nattch;     /* current number attached  */
    long        atime;      /* last attach time         */
    long        dtime;      /* last detach time         */
    long        ctime;      /* last change time         */

} SHM;

#define N_SHM       256

#define NIL_SHM     (SHM *)0

static SHM          shm[N_SHM];

static int          new_shm();
static int          find_shm();
static SHM          *which_shm();
static SHM          *which_dt_shm();
static void         release_shm();
static int          test_perm();
static int          test_bits();

#define ERROR(eno)  { errno = eno; return -1; }

/*------------------------------------------------------*/

void shm_init()

{
    int     i;

    id_init(SHM_ID);

    for ( i = 0; i < N_SHM; ++i )
        shm[i].cpid = 0;
}

/*------------------------------------------------------*/

int shm_get(pid, key, size, shmflg)

INT     pid;
key_t   key;
INT     size;
int     shmflg;

{
    SHORT       uid, gid, type;
    int         shmid;
    IPC_PERM    *pp;
    SHM         *sp;

    if ( size < PAGE_SIZE || size > MAXSIZE )
        ERROR(EINVAL)

    proc_getuid(pid, &uid, &gid);

    if ( key != IPC_PRIVATE )
    {
        shmid = find_shm(key);

        if ( shmid != -1 && (shmflg & IPC_EXCL) )
            ERROR(EEXIST)

        if ( shmid == -1 && (shmflg & IPC_CREAT) == 0 )
            ERROR(ENOENT)

        if ( shmid != -1 )
        {
            sp = &shm[shmid];

            if ( test_perm(&(sp->perm), shmflg, uid, gid) == FALSE )
                ERROR(EACCES)

            if ( size == 0 || size > sp->segsz )
                ERROR(EINVAL)

            return (int) shm[shmid].perm.seq; 
        }
    }
            
    if ( (shmid = new_shm(key)) == -1 )
        ERROR(ENOSPC)

    sp = &shm[shmid];
    pp = &(sp->perm);

    type = ( shmflg & SHM_RDONLY ) ? R_SHM_R : R_SHM_W;

    sp->rno = mm_alloc(type);
    sp->pde = mm_grow(sp->rno, size);

    pp->uid = pp->cuid = uid;
    pp->gid = pp->cgid = gid;

    pp->mode = (shmflg & 0777);
    pp->key  = key;
    pp->seq  = id_new(SHM_ID);

    sp->cpid   = (SHORT) pid;
    sp->segsz  = size; 
    sp->nattch = 0;
    sp->atime  = 0L;
    sp->dtime  = 0L;
    sp->ctime  = TIME();

    return (int) pp->seq;
}

/*------------------------------------------------------*/

ADDR shm_at(pid, shmid, shmaddr, shmflg)

INT     pid;
int     shmid;
ADDR    shmaddr;
int     shmflg;

{
    SHORT       uid, gid;
    SHM         *sp = which_shm(shmid);
    IPC_PERM    *pp;
    ADDR        addr;
    int         perms;

    if ( sp == NIL_SHM )
    {
        errno = EINVAL;
        return (ADDR)0;
    }

    pp = &sp->perm;

    proc_getuid(pid, &uid, &gid);

    perms = ( shmflg & SHM_RDONLY ) ? 0444 : 0666;

    if ( test_perm(pp, perms, uid, gid) == FALSE )
    {
        errno = EACCES;
        return (ADDR)0;
    }

    if ( shmaddr != 0 && (shmflg & SHM_RND) )
        addr = shmaddr - (INT) (shmaddr % SHMLBA);
    else
        addr = shmaddr;

    if ( (addr = mm_shmat(pid, sp->rno, sp->pde, addr, sp->segsz,
                                (shmflg & SHM_RDONLY))) == (ADDR)0 )
        return (ADDR)0;

    ++sp->nattch;

    sp->atime = TIME();

    return addr;
}

/*------------------------------------------------------*/

int shm_dt(pid, shmaddr)

INT     pid;
ADDR    shmaddr;

{
    SHM         *sp;
    int         rno;

    if ( (rno = mm_shmdt(pid, shmaddr)) == -1 )
        return -1;

    sp = which_dt_shm(rno);

    --sp->nattch;

    sp->dtime = TIME();

    return 0;
}

/*------------------------------------------------------*/

int shm_ctl(pid, shmid, cmd, buf)

INT      pid;
int      shmid;
int      cmd;
SHMID    *buf;

{
    SHORT       uid, gid, new_uid, new_gid;
    SHM         *sp = which_shm(shmid);
    IPC_PERM    *pp;

    if ( sp == NIL_SHM )
        ERROR(EINVAL)

    pp = &sp->perm;

    proc_getuid(pid, &uid, &gid);

    switch ( cmd )
    {
    case IPC_STAT:
        if ( test_perm(pp, 0444, uid, gid) == FALSE )
            ERROR(EACCES)

        (void) MEMCPY(&(buf->perm), pp, sizeof(IPC_PERM));
        buf->segsz  = (int) sp->segsz;
        buf->lpid   = sp->lpid;
        buf->cpid   = sp->cpid;
        buf->nattch = sp->nattch;
        buf->atime  = sp->atime;
        buf->dtime  = sp->dtime;
        buf->ctime  = sp->ctime;
        break;
    case IPC_SET:
        new_uid = (buf->perm).uid;
        new_gid = (buf->perm).gid;

        if ( uid != ROOT_UID && uid != pp->uid )
            ERROR(EPERM)

        if ( uid == ROOT_UID || new_uid == pp->cuid )
        {
            pp->uid  = new_uid;
            pp->gid  = new_gid;
        }
        else if ( new_gid == pp->cgid )
            pp->gid = new_gid;
        else
            ERROR(EINVAL)

        pp->mode  = ((buf->perm).mode) & 0777;
        sp->ctime = TIME();

        break;
    case IPC_RMID:
        if ( uid != ROOT_UID && uid != pp->uid )
            ERROR(EPERM)

        (void) mm_free(sp->rno);
        release_shm(sp);
        break;
    default:
        ERROR(EINVAL)
    }

    return 0;
}

/*--------------- Local functions ----------------------*/

static int  new_shm(key)

key_t   key;

{
    SHM *sp;

    for ( sp = shm; sp < shm + N_SHM; ++sp ) 
        if ( sp->cpid == 0 )
            break;

    sp->key = key;

    return ( sp < shm + N_SHM ) ? (sp - shm) : -1;
}

/*------------------------------------------------------*/

static int  find_shm(key)

key_t   key;

{
    SHM *sp;

    for ( sp = shm; sp < shm + N_SHM; ++sp ) 
        if ( sp->key == key )
            break;

    return ( sp < shm + N_SHM ) ? (sp - shm) : -1;
}

/*------------------------------------------------------*/

static SHM *which_shm(shmid)

int     shmid;

{
    SHM *sp;

    for ( sp = shm; sp < shm + N_SHM; ++sp ) 
        if ( sp->cpid != 0 &&
                        sp->perm.seq == (SHORT) shmid )
            break;

    return ( sp < shm + N_SHM ) ? sp : NIL_SHM;
}

/*------------------------------------------------------*/

static SHM *which_dt_shm(rno)

int     rno;

{
    SHM *sp;

    for ( sp = shm; sp < shm + N_SHM; ++sp ) 
        if ( sp->cpid != 0 && sp->rno == rno )
            break;

    return ( sp < shm + N_SHM ) ? sp : NIL_SHM;
}

/*------------------------------------------------------*/

static void release_shm(sp)

SHM     *sp;

{
    sp->cpid = 0;   
}

/*------------------------------------------------------*/

static int  test_perm(pp, flg, uid, gid)

IPC_PERM    *pp;  
int         flg;
SHORT       uid;
SHORT       gid;

{
    INT qmode = pp->mode & 0777;
    INT umode = (INT) (flg & 0777);

    if ( uid == pp->uid && test_bits((qmode >> 6), (umode >> 6)) )
        return TRUE;

    qmode &= 077;
    umode &= 077;

    if ( gid == pp->gid && test_bits((qmode >> 3), (umode >> 3)) )
        return TRUE;

    qmode &= 07;
    umode &= 07;

    return test_bits(qmode, umode);
}

/*------------------------------------------------------*/

static int test_bits(perm, req)

INT     perm;
INT     req;

{
    if ( (req & 4) && !(perm & 4) )
        return FALSE;

    if ( (req & 2) && !(perm & 2) )
        return FALSE;

    return TRUE;
}          
