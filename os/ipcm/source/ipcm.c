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
* This module manages the communication between the IPC
* manager and the rest of the world. It is essentially a long
* switch, calling the appropriate functions depending
* on the command contained in an incoming message.
*
***********************************************************
*/

#include    "../include/ktypes.h" 
#include    "../include/fcntl.h"
#include    "../include/page.h"
#include    "../include/msg.h"
#include    "../include/ipcmsg.h"
#include    "../include/sem.h"
#include    "../include/ipcsem.h"
#include    "../include/shm.h"
#include    "../include/ipcshm.h"
#include    "../include/proc.h"
#include    "../include/mmcall.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/chan.h"
#include    "../include/thread.h"
#include    "../include/fcodes.h"
#include    "../include/comm.h"
#include    "../include/kmsg.h"
#include    "../include/pid.h"

extern int  errno;
extern int  thread_cnt;

extern void perror();
extern int  open();
extern int  write();
extern int  close();

MSG msg;

int my_pid  = IM_PID;
int debug   = FALSE;  
int fdl     = -1; 
int quit    = FALSE;  
int critical; 

static int  start   = TRUE; 

void fatal();

#define reply(queue, class)     send((queue), &msg, (class))

/*-----------------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
    while ( argc > 1 && argv[1][0] == '-' )
    {
        switch ( argv[1][1] )
        {
        case 'd':
            debug = TRUE;
            fdl   = open("ipclog", O_WRONLY | O_CREAT | O_TRUNC, 0600);
            break;
        }

        --argc;
        ++argv;
    }

    comm_init();
    init_threads();

    while ( !quit )
    {
        critical = ( start || thread_cnt < 2 );

        if ( receive(&msg, critical) == -1 )
        {
            if ( errno == EINTR )
                continue;

            fatal();
        }
                                                     
        errno = 0;

        start_thread(); 
 
        switch ( msg.cmd )
        {
        case INIT:
            chan_init();
            msg_init();
            sem_init();
            shm_init();
            proc_init();
            start = FALSE;
            break;
        case FORK:
            {
                INT chan = msg.chan;

                proc_fork((INT) msg.arg1, (INT) msg.arg2);
                msg.cmd   = IPCDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case SETEID:
            {
                INT chan = msg.chan;

                proc_setuid((INT) msg.arg1, (SHORT) msg.arg2,
                                            (SHORT) msg.arg3);
                msg.cmd   = IPCDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break; 
        case EXIT:
            {
                INT chan = msg.chan;

                proc_exit((INT) msg.arg1);
                msg.cmd   = IPCDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case MSGCTL:
            {
                INT    pid = msg.pid;
                MSGID  qbuf;

                (void) MEMCPY(&qbuf, msg.str1, sizeof(MSGID));

                msg.arg1  = (ARGTYPE) msg_ctl(pid, (int) msg.arg1,
                                                    (int) msg.arg2, &qbuf);
                msg.errno = errno;

                (void) MEMCPY(msg.str1, &qbuf, sizeof(MSGID));

                reply(pid, UNCRITICAL);             
            }
            break;
        case MSGGET:
            {
                INT     pid  = msg.pid;

                msg.arg1  = (ARGTYPE) msg_get(pid, (key_t) msg.arg1,
                                                            (int) msg.arg2);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case MSGSND:
            {
                INT     pid  = msg.pid;
                INT     chan = msg.chan;

                msg.arg1  = (ARGTYPE) msg_snd(pid, (int) msg.arg1, chan,
                                            (long) msg.arg2, (INT) msg.arg3,
                                            (int) msg.arg4);
                msg.errno = errno;
                msg.chan  = chan;
                reply(pid, UNCRITICAL);
            }
            break;
        case MSGRCV:
            {
                INT     pid  = msg.pid;
                INT     chan = msg.chan;
                LONG    typ;

                msg.arg1  = (ARGTYPE) msg_rcv(pid, (int) msg.arg1, chan,
                                            (long) msg.arg2, (INT) msg.arg3,
                                            (int) msg.arg4, &typ);
                msg.errno = errno;
                msg.chan  = chan;
                msg.arg2  = (ARGTYPE) typ;
                reply(pid, UNCRITICAL);
            }
            break;
        case SEMCTL:
            {
                INT             pid = msg.pid;
                int             cmd = (int) msg.arg1;
                SEMID           sbuf;
                SHORT           array[MAXSEM];
                SEMUNION        arg;

                switch ( cmd )
                {
                case IPC_SET:
                    arg.buf = &sbuf;
                    (void) MEMCPY(&sbuf, msg.str1, sizeof(SEMID));
                    break;
                case IPC_STAT:
                    arg.buf = &sbuf;
                    break;
                case GETALL:
                    arg.array = array;
                    break;
                case SETALL:
                    arg.array = array;
                    (void) MEMCPY(array, msg.str1, MAXSEM * sizeof(SHORT));
                    break;
                default:
                    arg.val = (int) msg.arg2;
                }

                msg.arg1  = (ARGTYPE) sem_ctl(pid, (int) msg.arg3,
                                                (int) msg.arg4, cmd, arg);
                msg.errno = errno;

                switch ( cmd )
                {
                case IPC_STAT:
                    (void) MEMCPY(msg.str1, &sbuf, sizeof(SEMID));
                    break;
                case GETALL:
                    (void) MEMCPY(msg.str1, array, MAXSEM * sizeof(SHORT));
                    break;
                }

                reply(pid, UNCRITICAL);             
            }
            break;
        case SEMGET:
            {
                INT     pid  = msg.pid;

                msg.arg1  = (ARGTYPE) sem_get(pid, (key_t) msg.arg1,
                                            (int) msg.arg2, (int) msg.arg3);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case SEMOP:
            {
                INT      pid = msg.pid;
                SEMBUF   sbuf[MAXSEM];

                (void) MEMCPY(sbuf, msg.str1, MAXSEM * sizeof(SEMBUF));

                msg.arg1  = (ARGTYPE) sem_op(pid, (int) msg.arg1, sbuf,
                                                        (int) msg.arg2);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            } 
            break;
        case SHMCTL:
            {
                INT     pid = msg.pid;
                SHMID   sbuf;

                (void) MEMCPY(&sbuf, msg.str1, sizeof(SHMID));

                msg.arg1  = (ARGTYPE) shm_ctl(pid, (int) msg.arg1,
                                                    (int) msg.arg2, &sbuf);
                msg.errno = errno;

                (void) MEMCPY(msg.str1, &sbuf, sizeof(SHMID));
                reply(pid, UNCRITICAL);
            }
            break;
        case SHMGET:
            {
                INT         pid = msg.pid;

                msg.arg1  = (ARGTYPE) shm_get(pid, (key_t) msg.arg1,
                                            (INT) msg.arg2, (int) msg.arg3);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case SHMAT:
            {
                INT         pid = msg.pid;

                msg.arg1  = (ARGTYPE) shm_at(pid, (int) msg.arg1,
                                    (ADDR) msg.arg2, (int) msg.arg3);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case SHMDT:
            {
                INT         pid = msg.pid;

                msg.arg1  = (ARGTYPE) shm_dt(pid, (ADDR) msg.arg1);
                msg.errno = errno;

                reply(pid, UNCRITICAL);
            }
            break;
        case MMDONE:
            mm_done(msg.chan, (int)msg.arg1, msg.errno);
            break;
        case SHM_DONE:
            mm_done(msg.chan, (int)msg.arg1, msg.errno);
            break;
        case SHMAT_DONE:
            mm_adone(msg.chan, (ADDR)msg.arg1, msg.errno);
            break;
        default:
            break;
        }

        end_thread(); 
    }

    if ( debug )
        (void) close(fdl);

    exit(0); 
}

/*----------------------------------------------------*/

void fatal()

{
    perror("mm"); 
 
    if ( debug )
        (void) close(fdl);

    exit(1);
}

/*----------------------------------------------------*/


