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
* This module is an interim solution and a potpourri.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/ipc.h"
#include    "../include/signal.h"
#include    "../include/msg.h"
#include    "../include/kmsg.h"
#include    "../include/task.h"
#include    "../include/pid.h"
#include    "../include/macros.h"
#include    "../include/fcodes.h"
#include    "../include/kutil.h"
#include    "../include/errno.h"

extern int  write();
extern void perror();

extern int  errno;
extern int  quit;
extern int  int_active;
extern int  fdl; 
extern int  debug; 
 
extern MSG  *user_msg; 
 
static int  msg_k;
static int  msg_u;
static int  msg_i;

static int  data_k;
static int  data_u;

static MSG  kmsg;
static int  msgsz = sizeof(MSG) - sizeof(long);

static void cleanup();
static void int_hdlr();

/*--------------------------------------------------------------*/

void comm_init()

{
    (void) signal(SIGUSR1, cleanup);
    (void) signal(SIGUSR2, int_hdlr);

    msg_k = msgget((key_t) MSGKEY_K, IPC_CREAT | 0777);
    msg_u = msgget((key_t) MSGKEY_U, IPC_CREAT | 0777);
    msg_i = msgget((key_t) MSGKEY_I, IPC_CREAT | 0777);

    data_k = msgget((key_t) MSGKEY_K + DKEY_OFF, IPC_CREAT | 0777);
    data_u = msgget((key_t) MSGKEY_U + DKEY_OFF, IPC_CREAT | 0777);
}

/*--------------------------------------------------------------*/

void comm_end()

{
    (void) msgctl(msg_k, IPC_RMID, 0);
    (void) msgctl(msg_u, IPC_RMID, 0);
    (void) msgctl(msg_i, IPC_RMID, 0);

    (void) msgctl(data_k, IPC_RMID, 0);
    (void) msgctl(data_u, IPC_RMID, 0);
}

/*--------------------------------------------------------------*/

void panic(s)

char *s;

{
    extern int printf();

    (void) printf("PANIC: %s\n", s);
}

/*--------------------------------------------------------------*/

void enter_crit()

{}

/*--------------------------------------------------------------*/

void leave_crit()

{}

/*-----------------------------------------------------------*/

void send(m)

MSG *m;

{ 
    m->mtype = (long) m->queue;
    (void) msgsnd(msg_u, (struct msgbuf *)m, msgsz, 0);
 
    if ( debug )
        (void) write(fdl, (char *)m, sizeof(MSG));
}

/*--------------------------------------------------------------*/

int receive(queue)

INT queue;

{
    errno = 0; 

    (void) msgrcv(msg_k, (struct msgbuf *)user_msg, msgsz, (long) queue, 0);

    if ( errno == EINTR )
        return -1;
    else if ( errno != 0 )
    {
        perror("kernel: receive");
        return -1;
    }
    else if ( debug )
        (void) write(fdl, (char *)user_msg, sizeof(MSG));

    return 0;
}

/*--------------------------------------------------------------*/

void rcv_data(queue, dm)

INT         queue;
DATA_MSG    *dm;

{
    do
    {
        errno = 0;

        (void) msgrcv(data_k, (struct msgbuf *)dm, DATA_SIZE, (long)queue, 0);

    } while ( errno == EINTR );

    if ( errno != 0 )
        perror("kernel: rcv_data");
}

/*--------------------------------------------------------------*/

void snd_data(queue, dm)

INT         queue;
DATA_MSG    *dm;

{
    errno = 0;

    dm->mtype = (long)queue;

    (void) msgsnd(data_u, (struct msgbuf *)dm, DATA_SIZE, 0); 

    if ( errno != 0 && errno != EINTR )
        perror("kernel: snd_data");
}

/*---------------------------------------------------*/

void rcv_interrupt(imp)

IMSG    *imp;

{
    (void) msgrcv(msg_i, (struct msgbuf *)imp,
                sizeof(IMSG) - sizeof(long), 1L, 0);
}

/*---------------------------------------------------*/

void mask_interrupts()

{
}

/*---------------------------------------------------*/

void unmask_interrupts()

{
}

/*---------------------------------------------------*/

static INT  unix_pid[MAX_PID];

void register_pid(m)

MSG     *m;

{
    unix_pid[m->pid] = (INT) m->arg1;
}
    
/*---------------------------------------------------*/

void sig_hdlr(m)

MSG     *m;

{
    IMSG    imsg;

    kill(unix_pid[(int) m->arg1], SIGUSR2);

    imsg.mtype  = (long) m->arg1;
    imsg.int_no = (INT) m->arg2;

    (void) msgsnd(msg_i, (struct msgbuf *)&imsg,
                                    sizeof(IMSG) - sizeof(long), 0);
}

/*---------------------------------------------------*/

static void cleanup()

{
    (void) signal(SIGUSR1, cleanup);

    quit = TRUE;
}

/*---------------------------------------------------*/

static void int_hdlr()

{
    (void) signal(SIGUSR2, int_hdlr);  
 
    int_active = TRUE;
}



