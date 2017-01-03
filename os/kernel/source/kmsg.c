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
* This module manages the two kinds of messages used by the
* kernel to communicate with processes: kernel messages and
* data messages.
*
* This module actually is a flagrant illustration of the
* need for object oriented methods; every function occurs
* in two flavors -- one for kernel messages and one for
* data messages. One could of course have tried to write generic
* routines that were capable of handling generic messages,
* but somehow it didn't seem worth the trouble for only two
* classes of message. Maybe someday ...
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/kmsg.h"
#include    "../include/task.h"
#include    "../include/pid.h"
#include    "../include/kutil.h"

extern void panic();

#define N_MSGS      256
#define N_DMSGS     128

typedef struct node
{
    struct node *next;
    struct node *prev;
    MSG         m; 

} NODE;

#define NIL_NODE    (NODE *)0

static NODE     pool[N_MSGS];

typedef struct
{
    NODE    *first;
    NODE    *last;

} MLIST;

static MLIST    q[ALL_TASKS];

typedef struct dnode
{
    struct dnode    *next;
    struct dnode    *prev;
    INT             pid;            /* the sender of the data   */
    INT             chan;
    DATA_MSG        dm;

}  DNODE;

#define NIL_DNODE   (DNODE *)0

static DNODE    dpool[N_DMSGS];

typedef struct
{
    DNODE   *first;
    DNODE   *last;

} DLIST;

static DLIST    dq[ALL_TASKS];

/*---------------- Declaration of local functions -----------------*/

static void put_in();
static void take_out();
static void put_in_d();
static void take_out_d();

/*---------------- Definition of public functions -----------------*/

void msg_init()

{
    NODE    *n;
    MLIST   *ml;
    DNODE   *dn;
    DLIST   *dl;

    for ( n = pool; n < pool + N_MSGS; ++n )
        n->m.mtype = -1L;

    for ( ml = q; ml < q + ALL_TASKS; ++ml )
        ml->first = ml->last = NIL_NODE; 

    for ( dn = dpool; dn < dpool + N_DMSGS; ++dn )
        dn->dm.mtype = -1L;

    for ( dl = dq; dl < dq + ALL_TASKS; ++dl )
        dl->first = dl->last = NIL_DNODE;
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_alloc                                      */
/*                                                          */
/* Allocates a free kernel message.                         */
/*                                                          */
/************************************************************/

MSG *msg_alloc()

{
    NODE    *n;

    for ( n = pool; n < pool + N_MSGS; ++n )
        if ( n->m.mtype == -1L )
            break;

    if ( n >= pool + N_MSGS )
        return NIL_MSG;

    n->m.mtype = 0L;

    return &(n->m);
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_d_alloc                                    */
/*                                                          */
/* Allocates a free data message.                           */
/*                                                          */
/************************************************************/

DATA_MSG *msg_d_alloc()

{
    DNODE   *d;

    for ( d = dpool; d < dpool + N_DMSGS; ++d )
        if ( d->dm.mtype == -1L )
            break;

    if ( d >= dpool + N_DMSGS )
        return (DATA_MSG *)0;

    d->dm.mtype = 0L;

    return &(d->dm);
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_free                                       */
/*                                                          */
/* Returns a kernel message to the free pool.               */
/*                                                          */
/************************************************************/

void msg_free(m)

MSG     *m;

{
    m->mtype = -1L;
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_d_free                                     */
/*                                                          */
/* Returns a data message to the free pool.                 */
/*                                                          */
/************************************************************/

void msg_d_free(dm)

DATA_MSG    *dm;

{
    dm->mtype = -1L;
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_next                                       */
/*                                                          */
/* Returns the first kernel message from the queue belong-  */
/* ing to the task with number tno. The message is removed  */
/* from the queue.                                          */
/*                                                          */
/************************************************************/

MSG *msg_next(tno, critical)

INT     tno;
int     critical;

{
    MLIST   *ml = &q[tno];
    NODE    *n;

    for ( n = ml->first; n != NIL_NODE; n = n->next )
        if ( !critical || n->m.class == CRITICAL )
            break;

    if ( n == NIL_NODE )
        return NIL_MSG;

    take_out(ml, n);

    return &(n->m);
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_d_next                                     */
/*                                                          */
/* Returns the first data message from the queue belonging  */
/* to task with sender pid and channel number chan. The     */
/* message is removed from the queue.                       */
/*                                                          */
/************************************************************/

DATA_MSG *msg_d_next(task, pid, chan)

INT     task;
INT     pid;
INT     chan;

{
    DLIST   *dl = &dq[task];
    DNODE   *d;

    for ( d = dl->first; d != NIL_DNODE; d = d->next )
        if ( d->pid == pid && d->chan == chan )
            break; 

    if ( d == NIL_DNODE )
        return (DATA_MSG *)0;

    take_out_d(dl, d);

    return &(d->dm);
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_clear                                      */
/*                                                          */
/* Clears out all kernel messages still in the queue be-    */
/* longing to the task with number tno.                     */
/*                                                          */
/************************************************************/

void msg_clear(tno)

INT     tno;

{
    MLIST   *ml = &q[tno];
    NODE    *n;

    while ( (n = ml->first) != NIL_NODE )
    {
        take_out(ml, n);
        n->m.mtype = -1L;
    }
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_purge                                      */
/*                                                          */
/* Clears out all kernel messages still in the queue be-    */
/* longing to the task with number tno and coming from the  */
/* task with identifier pid.                                */
/*                                                          */
/************************************************************/

void msg_purge(tno, pid)

INT     tno;
INT     pid;

{
    MLIST   *ml = &q[tno];
    NODE    *n;

    while ( (n = ml->first) != NIL_NODE )
    {
        if ( n->m.pid == pid )
        {
            take_out(ml, n);
            n->m.mtype = -1L;
        }
        else
            n = n->next;
    }
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_d_clear                                    */
/*                                                          */
/* Clears out all data messages still in the queue belong-  */
/* ing to the task with number tno.                         */
/*                                                          */
/************************************************************/

void msg_d_clear(tno)

INT     tno;

{
    DLIST   *dl = &dq[tno];
    DNODE   *d;

    while ( (d = dl->first) != NIL_DNODE )
    {
        take_out_d(dl, d);
        d->dm.mtype = -1L;
    }
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_d_purge                                    */
/*                                                          */
/* Clears out all data messages still in the queue be-      */
/* longing to the task with number tno and coming from the  */
/* task with identifier pid.                                */
/*                                                          */
/************************************************************/

void msg_d_purge(tno, pid)

INT     tno;
INT     pid;

{
    DLIST   *dl = &dq[tno];
    DNODE   *d;

    while ( (d = dl->first) != NIL_DNODE )
    {
        if ( d->pid == pid )
        {
            take_out(dl, d);
            d->dm.mtype = -1L;
        }
        else
            d = d->next;
    }
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_append                                     */
/*                                                          */
/* Appends the kernel message m (allocated by msg_alloc) to */
/* the appropriate queue.                                   */
/*                                                          */
/************************************************************/

void msg_append(queue, m)

INT     queue;
MSG     *m;

{
    NODE    *n;

    for ( n = pool; n < pool + N_MSGS; ++n )
        if ( &(n->m) == m )
            break;

    if ( n >= pool + N_MSGS )
        panic("bad message");

    if ( m->queue == 0 )
        panic("bad queue");

    put_in(&q[queue], n);
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_prepend                                    */
/*                                                          */
/* Prepends the kernel message m (allocated by msg_alloc)   */
/* to the appropriate queue. Needed for sys_request.        */
/*                                                          */
/************************************************************/

void msg_prepend(queue, m)

INT     queue;
MSG     *m;

{
    NODE    *n;
    MLIST   *ml;

    for ( n = pool; n < pool + N_MSGS; ++n )
        if ( &(n->m) == m )
            break;

    if ( n >= pool + N_MSGS )
        panic("bad message");

    if ( m->queue == 0 )
        panic("bad queue");

    ml = &q[queue];

    enter_crit();

    n->next   = ml->first;
    n->prev   = NIL_NODE;

    if ( ml->first != NIL_NODE )
        ml->first->prev = n;

    ml->first = n;
    
    if ( ml->last == NIL_NODE )
        ml->last = n;

    leave_crit();
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_d_append                                   */
/*                                                          */
/* Appends the data message d (allocated by msg_d_alloc) to */
/* the appropriate queue.                                   */
/*                                                          */
/************************************************************/

void msg_d_append(task, pid, chan, d)

INT         task;
INT         pid;
INT         chan;
DATA_MSG    *d;

{
    DNODE   *dn;

    for ( dn = dpool; dn < dpool + N_DMSGS; ++dn )
        if ( &(dn->dm) == d )
            break;

    dn->pid  = pid;
    dn->chan = chan;

    put_in_d(&dq[task], dn);
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_there                                      */
/*                                                          */
/* Says whether there is a message on the queue of task.    */
/*                                                          */
/************************************************************/

int msg_there(task)

INT         task;

{
    MLIST   *ml = &q[task];

    return ( ml->first != NIL_NODE );
}

/*------------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: msg_count                                      */
/*                                                          */
/* Counts the messages in a queue; used for test purposes.  */
/*                                                          */
/************************************************************/

INT msg_count(task)

INT         task;

{
    MLIST   *ml = &q[task];
    NODE    *n  = ml->first;
    INT     cnt = 0;

    for ( n = ml->first; n != NIL_NODE; n = n->next )
        ++cnt;

    return cnt;
}

/*---------------- Definition of local functions ------------------*/
        
static void put_in(ml, n)

MLIST   *ml;
NODE    *n;

{
    enter_crit();

    if ( ml->last != NIL_NODE )
        ml->last->next = n;
    else
        ml->first = n;

    n->next  = NIL_NODE;
    n->prev  = ml->last;
    ml->last = n;

    leave_crit();
}

/*---------------------------------------------------------------*/

static void take_out(ml, n)

MLIST   *ml;
NODE    *n;

{
    enter_crit();

    if ( ml->first == n )
        ml->first = n->next;

    if ( ml->last == n )
        ml->last = n->prev;

    if ( n->prev != NIL_NODE )
        n->prev->next = n->next;

    if ( n->next != NIL_NODE )
        n->next->prev = n->prev;

    leave_crit();
}

/*---------------------------------------------------------------*/

static void put_in_d(dl, d)

DLIST   *dl;
DNODE   *d;

{
    enter_crit();

    if ( dl->last != NIL_DNODE )
        dl->last->next = d;
    else
        dl->first = d;

    d->next  = NIL_DNODE;
    d->prev  = dl->last;
    dl->last = d;

    leave_crit();
}

/*---------------------------------------------------------------*/

static void take_out_d(dl, d)

DLIST   *dl;
DNODE   *d;

{
    enter_crit();

    if ( dl->first == d )
        dl->first = d->next;

    if ( dl->last == d )
        dl->last = d->prev;

    if ( d->prev != NIL_DNODE )
        d->prev->next = d->next;

    if ( d->next != NIL_DNODE )
        d->next->prev = d->prev;

    leave_crit();
}



