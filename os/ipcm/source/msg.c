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
* This module implements the IPC messages.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/ipc.h"
#include    "../include/msg.h"
#include    "../include/ipcmsg.h"
#include    "../include/map.h"
#include    "../include/id.h"
#include    "../include/proc.h"
#include    "../include/comm.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/thread.h"

extern int      errno;

typedef struct msg
{ 
    struct msg  *next;      /* ptr to next message on q */ 
    long        type;       /* message type */ 
    INT         size;       /* message text size */ 
    char        *text;      /* message text map address */ 
 
} MESG; 
 
typedef struct
{
    IPC_PERM    perm;   /* operation permission struct */
    MESG        *first; /* ptr to first message on q */
    MESG        *last;  /* ptr to last message on q */
    SHORT       cbytes; /* current # bytes on q */
    SHORT       qnum;   /* # of messages on q */
    SHORT       qbytes; /* max # of bytes on q */
    SHORT       lspid;  /* pid of last msgsnd */
    SHORT       lrpid;  /* pid of last msgrcv */
    long        stime;  /* last msgsnd time */
    long        rtime;  /* last msgrcv time */
    long        ctime;  /* last change time */

} MSG_QUEUE;

#define N_QUEUE     10
#define N_MESG      256
#define MAX_BYTES   32000
#define MAX_MSG     4096

#define NIL_MESG    (MESG *)0
#define NIL_QUEUE   (MSG_QUEUE *)0

static MESG         m[N_MESG];
static MSG_QUEUE    q[N_QUEUE];
static INT          free_msgs;

static int          new_queue();
static int          find_queue();
static MSG_QUEUE    *which_queue();
static void         release_queue();
static MESG         *new_msg();
static MESG         *find_msg();
static void         release_msg();
static void         put_in();
static void         take_out();
static int          test_perm();
static int          test_bits();

#define ERROR(eno)  { errno = eno; return -1; }

/*------------------------------------------------------*/

void msg_init()

{
    int     i;

    map_init();
    id_init(MSG_ID);

    for ( i = 0; i < N_QUEUE; ++i )
        q[i].perm.key = (key_t)0;

    for ( i = 0; i < N_MESG; ++i )
        m[i].type = -1L;

    free_msgs = N_MESG;
}

/*------------------------------------------------------*/

int msg_get(pid, key, msgflg)

INT     pid;
key_t   key;
int     msgflg;

{
    SHORT       uid, gid;
    int         msgid;
    MSG_QUEUE   *qp;
    IPC_PERM    *pp;

    proc_getuid(pid, &uid, &gid);

    if ( key != IPC_PRIVATE )
    {
        msgid = find_queue(key);

        if ( msgid != -1 && (msgflg & IPC_EXCL) )
            ERROR(EEXIST)

        if ( msgid == -1 && (msgflg & IPC_CREAT) == 0 )
            ERROR(ENOENT)

        if ( msgid != -1 )
        {
            qp = &q[msgid];

            if ( test_perm(&(qp->perm), msgflg, uid, gid) == FALSE )
                ERROR(EACCES)
        }

        if ( msgid != -1 )
            return (int) q[msgid].perm.seq;
    }
            
    if ( (msgid = new_queue()) == -1 )
        ERROR(ENOSPC)

    qp = &q[msgid];
    pp = &(qp->perm);

    pp->uid = pp->cuid = uid;
    pp->gid = pp->cgid = gid;

    pp->mode = (msgflg & 0777);
    pp->key  = key;
    pp->seq  = id_new(MSG_ID);

    qp->qnum   = 0;
    qp->lspid  = 0;
    qp->lrpid  = 0;
    qp->rtime  = 0L;
    qp->stime  = 0L;
    qp->ctime  = TIME();
    qp->qbytes = MAX_BYTES;

    qp->first  = NIL_MESG;
    qp->last   = NIL_MESG;
    qp->cbytes = 0;

    return (int) pp->seq;
}

/*------------------------------------------------------*/

int msg_snd(pid, qid, chan, msgtyp, msgsz, msgflg)

INT     pid;
int     qid;
INT     chan;
long    msgtyp;
INT     msgsz;
int     msgflg;

{
    SHORT       uid, gid;
    MSG_QUEUE   *qp = which_queue(qid);
    IPC_PERM    *pp;
    MESG        *msg;

    if ( qp == NIL_QUEUE || msgtyp < 1L || msgsz > MAX_MSG )
        ERROR(EINVAL)

    pp = &qp->perm;

    proc_getuid(pid, &uid, &gid);

    if ( test_perm(pp, 0222, uid, gid) == FALSE )
        ERROR(EACCES)

    while ( qp->cbytes + msgsz > qp->qbytes )
    {
        if ( msgflg & IPC_NOWAIT )
            ERROR(EAGAIN)

        sleep((EVENT) &(qp->cbytes));

        if ( (qp->perm).key == (key_t)0 ||
                        (qp->perm).seq != qid ) /* queue has been removed */
            ERROR(EIDRM)
    }

    if ( (msg = new_msg(msgflg & IPC_NOWAIT)) == NIL_MESG )
        ERROR(EAGAIN)

    msg->type = msgtyp;
    msg->size = msgsz;
    msg->text = map_alloc(msgsz, (msgflg & IPC_NOWAIT));

    if ( msg->text == (char *)0 )
    {
        release_msg(msg);
        ERROR(EAGAIN)
    }

    data_in(pid, chan, (CHAR *)msg->text, msgsz);

    put_in(qp, msg);

    qp->cbytes += msgsz;
    qp->qnum   += 1;
    qp->lspid   = pid;
    qp->stime   = TIME();

    wakeup((EVENT) &(qp->qnum));

    return 0;
}

/*------------------------------------------------------*/

int msg_rcv(pid, qid, chan, msgtyp, msgsz, msgflg, ptyp)

INT     pid;
int     qid;
INT     chan;
long    msgtyp;
INT     msgsz; 
int     msgflg;
LONG    *ptyp;

{
    SHORT       uid, gid;
    MSG_QUEUE   *qp = which_queue(qid);
    IPC_PERM    *pp;
    MESG        *mp;
    INT         size;

    if ( qp == NIL_QUEUE )
        ERROR(EINVAL)

    pp = &qp->perm;

    proc_getuid(pid, &uid, &gid);

    if ( test_perm(pp, 0444, uid, gid) == FALSE )
        ERROR(EACCES)

    while ( (mp = find_msg(qp, msgtyp)) == NIL_MESG )
    {
        if ( msgflg & IPC_NOWAIT )
            ERROR(ENOMSG)

        sleep((EVENT) &(qp->qnum));

        if ( (qp->perm).key == (key_t)0 ||
                        (qp->perm).seq != qid ) /* queue has been removed */
            ERROR(EIDRM)
    }

    size = MIN(mp->size, msgsz);

    data_out(pid, chan, (CHAR *) mp->text, size);

    qp->cbytes -= mp->size;
    qp->qnum   -= 1;
    qp->lrpid   = pid;
    qp->rtime   = TIME();

    wakeup((EVENT) &(qp->cbytes));

    *ptyp = (LONG) mp->type;

    map_free(mp->text);
    release_msg(mp);

    return 0;
}

/*------------------------------------------------------*/

int msg_ctl(pid, qid, cmd, buf)

INT      pid;
int      qid;
int      cmd;
MSGID    *buf;

{
    SHORT       uid, gid, new_uid, new_gid;
    MSG_QUEUE   *qp = which_queue(qid);
    IPC_PERM    *pp;

    if ( qp == NIL_QUEUE )
        ERROR(EINVAL)

    pp = &qp->perm;

    proc_getuid(pid, &uid, &gid);

    switch ( cmd )
    {
    case IPC_STAT:
        if ( test_perm(pp, 0444, uid, gid) == FALSE )
            ERROR(EACCES)

        (void) MEMCPY(&(buf->perm), pp, sizeof(IPC_PERM));
        buf->qnum   = qp->qnum;
        buf->qbytes = qp->qbytes;
        buf->lspid  = qp->lspid;
        buf->lrpid  = qp->lrpid;
        buf->stime  = qp->stime;
        buf->rtime  = qp->rtime;
        buf->ctime  = qp->ctime;
        break;
    case IPC_SET:
        new_uid = (buf->perm).uid;
        new_gid = (buf->perm).gid;

        if ( uid != ROOT_UID && uid != pp->uid )
            ERROR(EPERM)

        if ( buf->qbytes > qp->qbytes && uid != ROOT_UID )
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

        pp->mode   = ((buf->perm).mode) & 0777;
        qp->qbytes = buf->qbytes;

        qp->ctime = TIME();

        break;
    case IPC_RMID:
        if ( uid != ROOT_UID && uid != pp->uid )
            ERROR(EPERM)

        release_queue(qp);
        break;
    default:
        ERROR(EINVAL)
    }

    return 0;
}

/*--------------- Local functions ----------------------*/

static MESG *new_msg(no_block)

int     no_block;

{
    MESG    *mp;

    while ( free_msgs == 0 )
    {
        if ( no_block )
            return NIL_MESG;

        sleep((EVENT) &free_msgs);
    }

    for ( mp = m; mp < m + N_MESG; ++mp )
        if ( mp->type == -1L )
            break;

    --free_msgs;

    return mp;
}

/*------------------------------------------------------*/

static void release_msg(mp)

MESG    *mp;

{
    mp->type = -1L;
    ++free_msgs;

    if ( free_msgs == 1 )
        wakeup((EVENT) &free_msgs);
}

/*------------------------------------------------------*/

static MESG *find_msg(qp, typ)

MSG_QUEUE   *qp;
long        typ;

{
    int     range    = ( typ < 0L );
    long    mtyp     = ( range ) ? -typ : typ;
    long    ftyp     = mtyp + 1;
    MESG    *prevp   = NIL_MESG;
    MESG    *fst     = NIL_MESG;
    MESG    *pre_fst = NIL_MESG;
    MESG    *mp;

    if ( qp->qnum == 0 )
        return NIL_MESG;        /* don't waste time in this case    */

    if ( mtyp == 0L )
        mp    = qp->first;
    else if ( !range )
    {
        for ( mp = qp->first; mp != NIL_MESG; prevp = mp, mp = mp->next )
            if ( mp->type == mtyp )
                break;
    }  
    else
    {
        for ( mp = qp->first; mp != NIL_MESG; prevp = mp, mp = mp->next )
            if ( mp->type < ftyp )
            {
                ftyp    = mp->type;
                fst     = mp;
                pre_fst = prevp;
            }

        mp    = fst;
        prevp = pre_fst;
    }
 
    if ( mp != NIL_MESG ) 
        take_out(qp, mp, prevp); 
 
    return mp; 
}

/*------------------------------------------------------*/

static void put_in(qp, mp)

MSG_QUEUE   *qp;
MESG        *mp;

{
    if ( qp->last == NIL_MESG )
        qp->first = mp;
    else
        qp->last->next = mp;

    mp->next = NIL_MESG;
    qp->last = mp;
}

/*------------------------------------------------------*/

static void take_out(qp, mp, prevp)

MSG_QUEUE   *qp;
MESG        *mp;
MESG        *prevp;

{
    if ( prevp == NIL_MESG )
        qp->first = mp->next;
    else
        prevp->next = mp->next;

    if ( qp->first == NIL_MESG )
        qp->last = NIL_MESG;
}

/*------------------------------------------------------*/

static int  new_queue()

{
    MSG_QUEUE   *qp;

    for ( qp = q; qp < q + N_QUEUE; ++qp ) 
        if ( qp->perm.key == (key_t) 0 )
            break;

    return ( qp < q + N_QUEUE ) ? (qp - q) : -1;
}

/*------------------------------------------------------*/

static int  find_queue(key)

key_t   key;

{
    MSG_QUEUE   *qp;

    for ( qp = q; qp < q + N_QUEUE; ++qp ) 
        if ( qp->perm.key == key )
            break;

    return ( qp < q + N_QUEUE ) ? (qp - q) : -1;
}

/*------------------------------------------------------*/

static MSG_QUEUE *which_queue(qid)

int     qid;

{
    MSG_QUEUE   *qp;

    for ( qp = q; qp < q + N_QUEUE; ++qp ) 
        if ( qp->perm.key != (key_t)0 &&
                        qp->perm.seq == (SHORT) qid )
            break;

    return ( qp < q + N_QUEUE ) ? qp : NIL_QUEUE;
}

/*------------------------------------------------------*/

static void release_queue(qp)

MSG_QUEUE   *qp;

{
    MESG    *mp;

    while ( (mp = find_msg(qp, 0L)) != NIL_MESG )
    {
        map_free(mp->text);
        release_msg(mp);
    }
  
    qp->perm.key = (key_t)0; 
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
