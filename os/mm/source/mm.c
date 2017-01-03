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
* This module manages the communication between the memory
* manager and the kernel. It is essentially a long switch,
* calling the appropriate functions in region depending on
* the command contained in a kernel message.
*
***********************************************************
*/

#include    "../include/ktypes.h" 
#include    "../include/page.h"
#include    "../include/fcntl.h"
#include    "../include/pregion.h"
#include    "../include/region.h"
#include    "../include/devcall.h"
#include    "../include/fmcall.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
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

int my_pid = MM_PID;

MSG msg;

int debug   = FALSE;  
int fdl     = -1; 
int quit    = FALSE;  
int critical; 

static int start = TRUE; 

void fatal(); 

#define reply(queue, class)     send((queue), &msg, (class))

/*-----------------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
    if ( argc > 1 && STRCMP(argv[1], "-d") == 0 )
    {
        debug = TRUE;
        fdl   = open("mmlog", O_WRONLY | O_CREAT | O_TRUNC, 0600);
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
            prg_init();
            start = FALSE;
            break;
        case FORK:
            {
                INT     chan = msg.chan;

                prg_fork((INT) msg.arg1, (INT) msg.arg2); 

                msg.arg1  = (ARGTYPE)0;
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case EXEC:
            {
                INT     chan  = msg.chan;
                char    path[64];
 
                (void) STRCPY(path, msg.str1);

                msg.arg1  = (ARGTYPE) prg_exec((INT)msg.arg1, path);
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case EXIT:
            {
                INT     chan = msg.chan;

                prg_exit((INT) msg.arg1);
                msg.cmd  = MMDONE;
                msg.chan = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case BRK:
            {
                INT     chan = msg.chan;

                msg.arg1  = (ARGTYPE) prg_brk((INT) msg.arg1,
                                            (ADDR) msg.arg2, (INT) msg.arg3);
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case SHMAT:
            {
                INT     chan = msg.chan;

                msg.arg1  = (ARGTYPE) prg_shmat((INT) msg.arg1, (int) msg.arg2,
                                    (ADDR) msg.arg3, (INT) msg.arg4,
                                    (INT) msg.arg5, (int) msg.arg6);
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(IM_PID, CRITICAL);
            }
            break;
        case SHMDT:
            {
                INT     chan = msg.chan;

                msg.arg1  = (ARGTYPE) prg_shmdt((INT) msg.arg1,
                                                        (ADDR) msg.arg2);
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(IM_PID, CRITICAL);
            }
            break;
        case ALLOC:
            {
                INT     chan  = msg.chan;

                msg.arg1  = (ARGTYPE) reg_alloc((LONG) msg.arg1,
                                                    (SHORT) msg.arg2);
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(IM_PID, CRITICAL);
            }
            break;
        case FREE:
            {
                INT chan = msg.chan;

                reg_free((int) msg.arg1);
                msg.arg1  = (ARGTYPE) 0;
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(IM_PID, CRITICAL);
            }
            break;
        case GROW:
            {
                INT     chan  = msg.chan;

                msg.arg1  = (ARGTYPE) reg_grow((int) msg.arg1, (INT) msg.arg2,
                                            (INT) msg.arg3, (BLKNO) msg.arg4);
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(IM_PID, CRITICAL);
            }
            break;
        case DUMP:
            {
                INT chan = msg.chan;

                prg_dump((INT) msg.arg1, (int) msg.arg2); 

                msg.arg1  = (ARGTYPE)0;
                msg.errno = errno;
                msg.cmd   = MMDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case VFAULT:
            {
                INT pid  = (INT) msg.arg1;

                msg.arg1  = (ARGTYPE) prg_vfault(pid, (INT) msg.arg2,
                                                        (INT) msg.arg3);
                msg.cmd = VFAULT;
                reply(0, KER_MSG);
            }
            break;
        case PFAULT:
            {
                INT pid  = (INT) msg.arg1;

                msg.arg1  = (ARGTYPE) prg_pfault(pid, (INT) msg.arg2,
                                                        (INT) msg.arg3);
                msg.cmd = PFAULT;
                reply(0, KER_MSG);
            }
            break;
        case PMXDONE:
            fm_xfinish(msg.chan, (LONG)msg.arg2, msg.errno);
            break;
        case AHDRDONE:
            fm_ahdr_done(msg.chan, msg.str2, msg.errno);
            break;
        case BLKSDONE:
            reg_iodone((INT) msg.arg2, msg.chan, (int) msg.arg1, msg.errno);
            break;
        case BLKNODONE:
            reg_blkdone(msg.chan, (int) msg.arg1, msg.errno);
            break;
        case DUMPDONE:
            reg_dumpdone(msg.chan, (int) msg.arg1, msg.errno);
            break;
        case OPEN_DONE:
            dev_op_done(msg.chan, msg.errno);
            break;
        case READ_DONE:
            dev_rd_done(msg.chan, msg.errno);
            break;
        case WRITE_DONE:
            dev_wr_done(msg.chan, msg.errno);
            break;
        default:
            break;
        }

        end_thread();

        if ( debug )
            reg_test();
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


