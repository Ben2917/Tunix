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
* This module manages communication with the kernel.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/fcntl.h"
#include    "../include/signal.h"
#include    "../include/comm.h" 
#include    "../include/ipc.h"
#include    "../include/msg.h"
#include    "../include/kmsg.h"
#include    "../include/fcodes.h"
#include    "../include/pid.h"
#include    "../include/errno.h"
#include    "../include/macros.h"

extern int  write();

extern int  my_pid;
extern int  fdl;
extern int  errno;

static DATA_MSG dmsg; 
 
int     msg_k;
int     msg_u;

static int  data_k;
static int  data_u;

static int      msgsz = sizeof(MSG) - sizeof(long);
static int      start = TRUE;

static MSG      lmsg = {0L, KER_MSG, 0, 0, 0, LISTEN};
static MSG      rmsg = {0L, KER_MSG, 0, 0, 0, READ_IN};
static MSG      wmsg = {0L, KER_MSG, 0, 0, 0, WRITE_OUT};

static void     rcv_data_msg();
static void     snd_data_msg();
static void     snd_null_msg();
static void     cleanup();

/*---------------------------------------------------*/

void comm_init()

{
    (void) signal(SIGUSR1, cleanup);

    msg_k  = begin_msg((key_t) MSGKEY_K); 
    msg_u  = begin_msg((key_t) MSGKEY_U); 
    data_k = begin_msg((key_t) MSGKEY_K + DKEY_OFF); 
    data_u = begin_msg((key_t) MSGKEY_U + DKEY_OFF); 
}
 
/*---------------------------------------------------*/

int begin_msg(key)

key_t   key;

{
    return msgget(key, 0777);
}

/*---------------------------------------------------*/

void send(queue, m, class)

INT     queue;
MSG     *m;
INT     class;

{
    m->mtype = (long) my_pid;
    m->class = class;
    m->queue = queue;
    m->pid   = my_pid;

    (void) msgsnd(msg_k, (struct msgbuf *)m, msgsz, 0);

    if ( fdl != -1 )
        (void) write(fdl, (char *)m, sizeof(MSG));
}

/*---------------------------------------------------*/

int receive(m, critical)

MSG     *m;
int     critical; 

{
    int res;

    if ( !start )
    {
        lmsg.mtype = (long)my_pid;
        lmsg.pid   = my_pid;
        lmsg.arg3  = (ARGTYPE) critical;
        (void) msgsnd(msg_k, (struct msgbuf *)&lmsg, msgsz, 0); 

        if ( fdl != -1 )
            (void) write(fdl, (char *)&lmsg, sizeof(MSG));
    }

    start = FALSE;

    do res = msgrcv(msg_u, (struct msgbuf *)m, msgsz, (long)my_pid, 0); 
    while ( res == -1 && errno == EINTR );

    if ( fdl != -1 && res != -1 )
        (void) write(fdl, (char *)m, sizeof(MSG));

    return res;
}

/*---------------------------------------------------*/

void suppress()

{
    start = TRUE;           /* don't do a LISTEN on next receive    */
}

/*---------------------------------------------------*/

void data_in(pid, chan, buf, nbytes)

INT     pid;
INT     chan;
CHAR    *buf;
INT     nbytes;

{
    INT     done;
    INT     amnt = 0;       /* because of lint! */
    CHAR    *bp;

    for ( done = 0, bp = buf; done < nbytes;
                                done += amnt, bp += amnt )
    {
        amnt = MIN(nbytes - done, DATA_SIZE);
        rcv_data_msg(pid, chan, bp, amnt);
    }
}       

/*---------------------------------------------------*/

void data_out(pid, chan, buf, nbytes)

INT     pid;
INT     chan;
CHAR    *buf;
INT     nbytes;

{
    INT     done;
    CHAR    *bp;

    for ( done = 0, bp = buf; done < nbytes;
                                    done += DATA_SIZE, bp += DATA_SIZE )
        snd_data_msg(pid, chan, bp);
}       

/*---------------------------------------------------*/

void data_null(pid, chan, nbytes)

INT     pid;
INT     chan;
INT     nbytes;

{
    INT     done;

    for ( done = 0; done < nbytes; done += DATA_SIZE )
        snd_null_msg(pid, chan);
}

/*---------------------------------------------------*/

static void rcv_data_msg(pid, chan, buf, nbytes)

INT     pid;
INT     chan;
CHAR    *buf;
INT     nbytes;

{
    rmsg.mtype = (long)my_pid;
    rmsg.pid   = my_pid;
    rmsg.chan  = chan;
    rmsg.arg1  = (ARGTYPE) pid;
    (void) msgsnd(msg_k, (struct msgbuf *) &rmsg, msgsz, 0);

    if ( fdl != -1 )
        (void) write(fdl, (char *)&rmsg, sizeof(MSG));

    do
    {
        errno = 0;
        (void) msgrcv(data_u, (struct msgbuf *) &dmsg, DATA_SIZE,
                                                    (long)my_pid, 0);
    } while ( errno == EINTR );

    if ( buf != (CHAR *)0 )
        (void) MEMCPY(buf, dmsg.data, nbytes);
}

/*---------------------------------------------------*/

static void snd_data_msg(pid, chan, buf)

INT     pid;
INT     chan;
CHAR    *buf;

{
    if ( buf != (CHAR *)0 )
        (void) MEMCPY(dmsg.data, buf, DATA_SIZE);

    wmsg.mtype = (long)my_pid;
    wmsg.pid   = my_pid;
    wmsg.chan  = chan;
    wmsg.arg1  = (ARGTYPE) pid;
    (void) msgsnd(msg_k, (struct msgbuf *) &wmsg, msgsz, 0);

    if ( fdl != -1 )
        (void) write(fdl, (char *)&wmsg, sizeof(MSG));

    dmsg.mtype = (long) my_pid;
    (void) msgsnd(data_k, (struct msgbuf *) &dmsg, DATA_SIZE, 0);
}

/*---------------------------------------------------*/

static void snd_null_msg(pid, chan)

INT     pid;
INT     chan;

{
    (void) MEMSET(dmsg.data, '\0', DATA_SIZE);

    wmsg.mtype = (long)my_pid;
    wmsg.pid   = my_pid;
    wmsg.chan  = chan;
    wmsg.arg1  = (ARGTYPE) pid;
    (void) msgsnd(msg_k, (struct msgbuf *) &wmsg, msgsz, 0);

    if ( fdl != -1 )
        (void) write(fdl, (char *)&wmsg, sizeof(MSG));

    dmsg.mtype = (long) my_pid;
    (void) msgsnd(data_k, (struct msgbuf *) &dmsg, DATA_SIZE, 0);
}

/*---------------------------------------------------*/

static void cleanup(sig)

int sig;

{
    exit(0);
}

/*----------------------------------------------------*/

void pause_request(tno, m)

INT     tno;
MSG     *m;

{
    send(tno, m, UNCRITICAL);

    lmsg.mtype = (long)my_pid;
    lmsg.pid   = my_pid;
    lmsg.arg3  = (ARGTYPE) UNCRITICAL;
    (void) msgsnd(msg_k, (struct msgbuf *)&lmsg, msgsz, 0); 

    pause();
    (void) msgrcv(msg_u, (struct msgbuf *)m, msgsz, (long)my_pid, 0); 
}


