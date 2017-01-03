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
* This module manages the communication between the driver
* and the kernel. It is essentially a long switch,
* calling the appropriate functions depending on
* the command contained in a kernel message.
*
***********************************************************
*/

#include    "../include/ktypes.h" 
#include    "../include/kcall.h"
#include    "../include/page.h"
#include    "../include/fcntl.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/thread.h"
#include    "../include/fcodes.h"
#include    "../include/kmsg.h"
#include    "../include/pid.h"
#include    "../include/device.h"
#include    "../include/comm.h"
#include	"../include/signal.h"

extern int  errno;
extern int  thread_cnt;
extern char logname[];

extern void perror();
extern int  open();
extern int  write();
extern int  close();
extern int  sscanf();
extern int  sprintf();

MSG msg;
 
int maj;
int my_pid;

int quit  = FALSE;
int debug = FALSE;  
int fdl   = -1;

void fatal();

#define     talk(queue, class)      send((queue), &msg, (class))

static void sig_catch();

/*-----------------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
	(void) signal (SIGBUS, sig_catch); 
	(void) signal (SIGSEGV, sig_catch);

    while ( argc > 1 && argv[1][0] == '-' )
    {
        switch ( argv[1][1] )
        {
        case 'm': 
            (void) sscanf(&argv[1][2], "%d", &maj); 
            break; 
        case 'd':
            debug = TRUE;
            fdl   = open(logname, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            break;
        }

        --argc;
        ++argv;
    }

    my_pid = MAX_PID + maj;

    comm_init();
    init_threads();

    while ( !quit )
    {
        if ( thread_cnt < 2 )
            panic("running out of threads");

        if ( receive(&msg, FALSE) == -1 )
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
            dev_init();
            break;
        case OPEN:
            {
                INT     pid  = msg.pid;

                msg.arg1  = (ARGTYPE) dev_open((DEV) msg.arg1);
                msg.errno = errno;
                msg.cmd   = OPEN_DONE;
                talk(pid, CRITICAL);
            }
            break;
        case CLOSE:
            dev_close((SHORT) msg.arg1);
            break;
        case READ:
            {
                INT     chan = msg.chan;
                INT     pid  = msg.pid;
                DEV     dev  = (DEV) msg.arg1;

                msg.arg1  = (ARGTYPE) dev_read(dev, (LONG) msg.arg2,
                                                    (INT) msg.arg3,
                                                                chan, pid);
                msg.errno = errno;
                msg.chan  = chan;
                msg.cmd   = READ_DONE;
                msg.arg2  = (ARGTYPE) dev;
                talk(pid, CRITICAL);
            }
            break;
        case WRITE:
            {
                INT     chan = msg.chan;
                INT     pid  = msg.pid;

                msg.arg1  = (ARGTYPE) dev_write((DEV) msg.arg1,
                                    (LONG) msg.arg2, (INT) msg.arg3, chan);
                msg.errno = errno;
                msg.chan  = chan;
                msg.cmd   = WRITE_DONE;  

                talk(pid, CRITICAL);
            }
            break;
        case IOCTL:
            {
                INT     chan = msg.chan;
                int     size;

                msg.arg1  = (ARGTYPE) dev_ioctl((DEV) msg.arg1,
                                        (int) msg.arg2, msg.str1, &size);
                msg.errno = errno;
                msg.chan  = chan;
                msg.cmd   = IOCTL_DONE;  
                msg.arg1  = (ARGTYPE) size;

                talk(FM_PID, CRITICAL);
            }
            break;
        case INTR:
            dev_intr((INT) msg.arg1);
            break;
        default:
            break;
        }

        end_thread();
    }

    dev_close_all();

    if ( debug )
        (void) close(fdl);

    exit(0); 
}

/*----------------------------------------------------*/

void fatal()

{
    perror("driver"); 
 
    if ( debug )
        (void) close(fdl);

    exit(1);
}

/*----------------------------------------------------*/

void sig_catch (s)

int s;

{
	printf ("signal of type %d caught\n", s);
	exit (-1);
}

