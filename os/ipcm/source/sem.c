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
* This module implements the IPC semaphores.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/ipc.h"
#include    "../include/sem.h"
#include    "../include/ipcsem.h"
#include    "../include/map.h"
#include    "../include/id.h"
#include    "../include/proc.h"
#include    "../include/comm.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/thread.h"

extern int      errno;

typedef struct
{
    SHORT   val;        /* value of semaphore       */
    SHORT   pid;        /* pid of last operation    */
    SHORT   ncnt;       /* # awaiting for increase  */
    SHORT   zcnt;       /* # awaiting val = 0       */

} SEM;

typedef struct
{
    IPC_PERM    perm;       /* operation permission struct  */
    SEM         *set;       /* array of semaphores          */
    SHORT       nsems;      /* # of semaphores in set       */
    long        otime;      /* last semop time              */
    long        ctime;      /* last change time             */

} SEM_SET;

#define N_SEM       256

#define NIL_SEM     (SEM *)0
#define NIL_SEMSET  (SEM_SET *)0

static SEM_SET      s[N_SEM];

static int          new_sem_set();
static int          find_sem_set();
static SEM_SET      *which_sem_set();
static void         release_sem_set();
static int          test_perm();
static int          test_bits();

#define ERROR(eno)  { errno = eno; return -1; }

/*------------------------------------------------------*/

void sem_init()

{
    int     i;

    id_init(SEM_ID);

    for ( i = 0; i < N_SEM; ++i )
        s[i].nsems = 0;
}

/*------------------------------------------------------*/

int sem_get(pid, key, count, semflg)

INT     pid;
key_t   key;
int     count;
int     semflg;

{
    SHORT       uid, gid;
    int         semid, i;
    SEM_SET     *sp;
    IPC_PERM    *pp;
    SEM         *sm;

    if ( count <= 0 || count > MAXSEM )
        ERROR(EINVAL)

    proc_getuid(pid, &uid, &gid);

    if ( key != IPC_PRIVATE )
    {
        semid = find_sem_set(key);

        if ( semid != -1 && (semflg & IPC_EXCL) )
            ERROR(EEXIST)

        if ( semid == -1 && (semflg & IPC_CREAT) == 0 )
            ERROR(ENOENT)

        if ( semid != -1 )
        {
            sp = &s[semid];

            if ( test_perm(&(sp->perm), semflg, uid, gid) == FALSE )
                ERROR(EACCES)
        }

        if ( semid != -1 )
            return (int) s[semid].perm.seq;
    }
            
    if ( (semid = new_sem_set()) == -1 )
        ERROR(ENOSPC)

    sp = &s[semid];
    pp = &(sp->perm);

    pp->uid = pp->cuid = uid;
    pp->gid = pp->cgid = gid;

    pp->mode = (semflg & 0777);
    pp->key  = key;
    pp->seq  = id_new(SEM_ID);

    sp->set   = (SEM *) map_alloc((INT)(count * sizeof(SEM)), TRUE);

    if ( sp->set == NIL_SEM )
        ERROR(ENOSPC)

    sp->nsems = count; 
    sp->otime = 0L;
    sp->ctime = TIME();

    for ( i = 0, sm = sp->set; i < count; ++i, ++sm )
    {
        sm->val  = 0;
        sm->pid  = 0;
        sm->ncnt = 0;
        sm->zcnt = 0;
    }

    return (int) pp->seq;
}

/*------------------------------------------------------*/

int sem_op(pid, semid, sops, nsops)

INT      pid;
int      semid;
SEMBUF   *sops;
int      nsops;

{
    SHORT       uid, gid;
    SEM_SET     *sp = which_sem_set(semid);
    IPC_PERM    *pp;
    SEM         *sm;
    SEMBUF      *sb;
    int         i;

    if ( sp == NIL_SEMSET )
        ERROR(EINVAL)

    pp = &sp->perm;

start:
    proc_getuid(pid, &uid, &gid);

    if ( test_perm(pp, 0222, uid, gid) == FALSE )
        ERROR(EACCES)

    for ( i = 0, sb = sops; i < nsops; ++i, ++sb )
    {
        sm = sp->set + sb->num;

        if ( sb->op < (short)0 && -(sb->op) > (short)sm->val )
        {
            if ( (sb->flg) & IPC_NOWAIT )
                ERROR(EAGAIN)

            ++sm->ncnt;
            sleep((EVENT) &(sm->ncnt));
            --sm->ncnt;

            if ( (sp = which_sem_set(semid)) == NIL_SEMSET )
                ERROR(EIDRM)

            goto start;
        }

        if ( sb->op == 0 && sm->val != 0 )
        {
            if ( (sb->flg) & IPC_NOWAIT )
                ERROR(EAGAIN)

            ++sm->zcnt;
            sleep((EVENT) &(sm->zcnt));
            --sm->zcnt;

            if ( (sp = which_sem_set(semid)) == NIL_SEMSET )
                ERROR(EIDRM)

            goto start;
        } 
    }

    /* If we get here, all operations can be successfully carried out. */

    for ( i = 0, sb = sops; i < nsops; ++i, ++sb )
    {
        sm       = sp->set + sb->num;
        sm->val += sb->op;
        sm->pid  = pid;

        if ( sb->op > 0 && sm->ncnt > 0 )
            wakeup((EVENT) &(sm->ncnt));

        if ( sm->val == 0 && sm->zcnt > 0 )
            wakeup((EVENT) &(sm->zcnt));

        if ( (sb->flg & SEM_UNDO) &&
                proc_adj(pid, semid, sb->num, sb->op) == -1 )
            ERROR(ENOSPC)
    }

    sp->otime = TIME();

    return 0;
}

/*------------------------------------------------------*/

int sem_ctl(pid, semid, semnum, cmd, arg)

INT             pid;
int             semid;
int             semnum;
int             cmd;
SEMUNION        arg;

{
    SHORT       uid, gid, new_uid, new_gid, *vp;
    SEM_SET     *sp = which_sem_set(semid);
    IPC_PERM    *pp;
    SEM         *sm;
    int         i;

    if ( sp == NIL_SEMSET )
        ERROR(EINVAL)

    pp = &sp->perm;

    proc_getuid(pid, &uid, &gid);

    switch ( cmd )
    {
    case IPC_STAT:
        if ( test_perm(pp, 0444, uid, gid) == FALSE )
            ERROR(EACCES)

        (void) MEMCPY(&(arg.buf->perm), pp, sizeof(IPC_PERM));
        arg.buf->nsems = sp->nsems;
        arg.buf->otime = sp->otime;
        arg.buf->ctime = sp->ctime;
        break;
    case IPC_SET:
        new_uid = (arg.buf->perm).uid;
        new_gid = (arg.buf->perm).gid;

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

        pp->mode   = ((arg.buf->perm).mode) & 0777;

        sp->ctime = TIME();

        break;
    case IPC_RMID:
        if ( uid != ROOT_UID && uid != pp->uid )
            ERROR(EPERM)

        release_sem_set(sp);
        break;
    case GETNCNT:
        if ( test_perm(pp, 0444, uid, gid) == FALSE )
            ERROR(EACCES)

        if ( semnum < 0 || semnum >= (int)sp->nsems )
            ERROR(EINVAL)

        sm = sp->set + semnum;
        return (int) sm->ncnt;
    case GETPID:
        if ( test_perm(pp, 0444, uid, gid) == FALSE )
            ERROR(EACCES)

        if ( semnum < 0 || semnum >= (int)sp->nsems )
            ERROR(EINVAL)

        sm = sp->set + semnum;
        return (int) sm->pid;
    case GETVAL:
        if ( test_perm(pp, 0444, uid, gid) == FALSE )
            ERROR(EACCES)

        if ( semnum < 0 || semnum >= (int)sp->nsems )
            ERROR(EINVAL)

        sm = sp->set + semnum;
        return (int) sm->val;
    case GETALL:
        if ( test_perm(pp, 0444, uid, gid) == FALSE )
            ERROR(EACCES)

        for ( i = 0, vp = arg.array, sm = sp->set; i < (int)sp->nsems; ++i )
            *vp++ = (sm++)->val;

        break;
    case GETZCNT:
        if ( test_perm(pp, 0444, uid, gid) == FALSE )
            ERROR(EACCES)

        if ( semnum < 0 || semnum >= (int)sp->nsems )
            ERROR(EINVAL)

        sm = sp->set + semnum;
        return (int) sm->zcnt;
    case SETVAL:
        if ( test_perm(pp, 0222, uid, gid) == FALSE )
            ERROR(EACCES)

        if ( semnum < 0 || semnum >= (int)sp->nsems )
            ERROR(EINVAL)

        sm = sp->set + semnum;
        sm->val = arg.val;
        
        sp->ctime = TIME();
        break;
    case SETALL:
        if ( test_perm(pp, 0222, uid, gid) == FALSE )
            ERROR(EACCES)

        for ( i = 0, vp = arg.array, sm = sp->set; i < (int)sp->nsems; ++i )
            (sm++)->val = *vp++;

        sp->ctime = TIME();
        break;
    default:
        ERROR(EINVAL)
    }

    return 0;
}

/*------------------------------------------------------*/

void sem_adj(semid, semnum, adjval)

int     semid;
SHORT   semnum;
short   adjval;

{
    SEM_SET     *sp = which_sem_set(semid);
    SEM         *sm = sp->set + semnum;

    sm->val += adjval;
}

/*--------------- Local functions ----------------------*/

static int  new_sem_set()

{
    SEM_SET *sp;

    for ( sp = s; sp < s + N_SEM; ++sp ) 
        if ( sp->nsems == 0 )
            break;

    return ( sp < s + N_SEM ) ? (sp - s) : -1;
}

/*------------------------------------------------------*/

static int  find_sem_set(key)

key_t   key;

{
    SEM_SET *sp;

    for ( sp = s; sp < s + N_SEM; ++sp ) 
        if ( sp->perm.key == key )
            break;

    return ( sp < s + N_SEM ) ? (sp - s) : -1;
}

/*------------------------------------------------------*/

static SEM_SET *which_sem_set(semid)

int     semid;

{
    SEM_SET *sp;

    for ( sp = s; sp < s + N_SEM; ++sp ) 
        if ( sp->nsems != 0 &&
                        sp->perm.seq == (SHORT) semid )
            break;

    return ( sp < s + N_SEM ) ? sp : NIL_SEMSET;
}

/*------------------------------------------------------*/

static void release_sem_set(sp)

SEM_SET *sp;

{
    SEM     *sm;
    int     i;

    for ( i = 0, sm = sp->set; i < (int)sp->nsems; ++i, ++sm )
    {
        if ( sm->ncnt )
            wakeup((EVENT) &(sm->ncnt));
        if ( sm->zcnt )
            wakeup((EVENT) &(sm->zcnt));
    }

    map_free((char *)sp->set);

    sp->nsems = 0;  
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
