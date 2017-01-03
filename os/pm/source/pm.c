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
* This module manages the communication between the process
* manager and the rest of the world. It is essentially a long
* switch, calling the appropriate functions in proc depending
* on the command contained in an incoming message.
*
***********************************************************
*/

#include    "../include/ktypes.h" 
#include    "../include/page.h"
#include    "../include/fcntl.h"
#include    "../include/times.h"
#include    "../include/signal.h" 
#include    "../include/proc.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/thread.h"
#include    "../include/fcodes.h"
#include    "../include/kcall.h"
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

int my_pid  = PM_PID;
int debug   = FALSE;  
int fdl     = -1; 
int quit    = FALSE;  
int critical; 

static int  show; 
static int  start   = TRUE; 
static int  verbose = FALSE; 

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
            fdl   = open("pmlog", O_WRONLY | O_CREAT | O_TRUNC, 0600);
            break;
        case 'v':
            verbose = TRUE;
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
            proc_init();
            start = FALSE;
            break;
        case FORK:
            {
                INT pid = msg.pid;
                int new_pid;

                new_pid   = proc_fork(pid);
                msg.arg1  = (ARGTYPE) new_pid;
                msg.errno = errno;
                reply(pid, UNCRITICAL);
                if ( new_pid != -1 )
                {
                    msg.cmd  = FORK;
                    msg.arg1 = (ARGTYPE) 0;
                    msg.arg2 = (ARGTYPE) new_pid;
                    reply((INT) new_pid, UNCRITICAL);
                }
            }
            show = TRUE;
            break;
        case EXEC:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);

                if ( pid == 0 )     /* it's the kernel  */
                    pid = INIT_PID;

                msg.arg1  = (ARGTYPE) proc_exec(pid, path);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case EXIT:
            proc_exit(msg.pid, (INT) msg.arg1); 
            show = TRUE;
            break;
        case PAUSE:
            {
                INT     pid = msg.pid;

                if ( proc_pause(pid) )
                {
                    msg.arg1  = (ARGTYPE) -1;
                    msg.errno = errno;
                    reply(pid, UNCRITICAL);
                }
            }
            show = TRUE;
            break;
        case WAIT:
            {
                INT     pid = msg.pid;
                INT     stat; 

                if ( (msg.arg1 = (ARGTYPE) proc_wait(pid, &stat)) != 0 )
                {
                    msg.errno = errno;
                    msg.arg2  = (ARGTYPE) stat;
                    reply(pid, UNCRITICAL);
                }
            }
            show = TRUE;
            break;
        case GETPPID:
            {
                INT     pid = msg.pid;
                INT     ppid, pgid;

                proc_getpid(pid, &ppid, &pgid);
                msg.arg1  = (ARGTYPE) 0;
                msg.errno = errno;
                msg.arg2  = (ARGTYPE) ppid;
                msg.arg3  = (ARGTYPE) pgid;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case GETUID:
            {
                INT     pid = msg.pid;
                SHORT   uid, gid, euid, egid;

                proc_getuid(pid, &uid, &gid, &euid, &egid);
                msg.arg1  = (ARGTYPE) 0;
                msg.errno = errno;
                msg.arg2  = (ARGTYPE) uid;
                msg.arg3  = (ARGTYPE) gid;
                msg.arg4  = (ARGTYPE) euid;
                msg.arg5  = (ARGTYPE) egid;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case NICE:
            {
                INT     pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_nice(pid, (INT) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case SETPGRP:
            {
                INT     pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_setpgrp(pid);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case SETUID:
            {
                INT     pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_setuid(pid, (SHORT) msg.arg1,
                                                        (SHORT) msg.arg2);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case TIMES:
            {
                INT         pid = msg.pid;
                struct tms  t;

                proc_times(pid, &t);
                msg.arg1  = (ARGTYPE) 0;
                msg.errno = errno;
                (void) MEMCPY(msg.str1, &t, sizeof(t));
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case KILL:
            {
                INT     pid = msg.pid;

                if ( pid )
                    msg.arg1 = (ARGTYPE) proc_kill(pid, (int) msg.arg1,
                                                            (INT) msg.arg2);
                else
                    msg.arg1 = (ARGTYPE) proc_kill(INIT_PID, (int) msg.arg1,
                                                            (INT) msg.arg2);
                msg.errno = errno;
                msg.cmd   = KILL;
                reply(pid, (pid) ? UNCRITICAL : KER_MSG);
            }
            show = TRUE;
            break;
        case SIGNAL:
            {
                INT     pid = msg.pid;
                HDLR    h   = (HDLR) msg.arg2;
                
                h = proc_signal(pid, (int) msg.arg1, h);

                msg.arg2  = (ARGTYPE) h;
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case ALARM:
            {
                INT     pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_alarm(pid, (INT) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case BRK:
            {
                INT     pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_brk(pid, (ADDR) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            show = TRUE;
            break;
        case SLEEP:
            {
                int     sig;

                msg.arg1 = (ARGTYPE) proc_sleep(&sig, (INT)msg.arg1);
                msg.cmd  = SLEEP;  
                msg.arg2 = (ARGTYPE) sig;
                reply(0, KER_MSG);
            }
            show = TRUE;
            break;
        case WAKEUP:
            {
                int     sig;

                msg.arg1 = (ARGTYPE) proc_wakeup((INT) msg.arg1, &sig,
                                                            (INT) msg.arg2);
                msg.cmd  = WAKEUP;  
                msg.arg2 = (ARGTYPE) sig;
                reply(0, KER_MSG);
            }
            show = TRUE;
            break;
        case TICK:
            {
                int sig;

                msg.arg1 = (ARGTYPE) proc_tick(&sig, (INT) msg.arg1);

                msg.cmd  = TICK; 
                msg.arg2 = (ARGTYPE) sig;
                reply(0, KER_MSG);
            }
            show = TRUE;
            break;
        case PMDONE:
            proc_fdone(msg.chan, (int)msg.arg1, msg.errno);
            show = FALSE;
            break;
        case PMDDONE: 
            proc_fdone(msg.chan, (int)msg.arg1, msg.errno);
            show = FALSE;
            break;
        case PMSDONE:
            proc_sdone(msg.chan, msg.str2, msg.errno);
            show = FALSE;
            break;
        case MMDONE:
            proc_mdone(msg.chan, (int)msg.arg1, msg.errno);
            show = FALSE;
            break;
        case ACT_ON_SIG:
            proc_act_on_sig((INT) msg.arg1);
            show = TRUE;
            break;
        case IPCDONE:
            proc_idone(msg.chan, (int)msg.arg1, msg.errno);
            show = FALSE;
            break;
        default:
            break;
        }

        if ( verbose && show )
            proc_stat();
  
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


