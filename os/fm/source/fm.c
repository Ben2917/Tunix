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
* This module manages the communication between the file
* manager and the client processes. It is essentially a
* giant switch, calling the appropriate functions in fproc
* depending on the command contained in a client message.
*
***********************************************************
*/

#include    "../include/ktypes.h" 
#include    "../include/fcntl.h"
#include    "../include/comm.h"
#include    "../include/fproc.h"
#include    "../include/ustat.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/thread.h"
#include    "../include/fcodes.h"
#include    "../include/devcall.h"
#include    "../include/kmsg.h"
#include    "../include/ahdr.h"
#include    "../include/pid.h"

extern int  errno;
extern int  thread_cnt;

extern void perror();
extern int  open();
extern int  write();
extern int  close();

int my_pid = FM_PID;
int quit   = FALSE; 

MSG msg;

int fdl   = -1; 
int debug = FALSE; 
 
static int start = TRUE;
static int critical; 

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
        fdl = open("fmlog", O_WRONLY | O_CREAT | O_TRUNC, 0600);
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
        case OPEN:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);
                msg.arg1  = (ARGTYPE) proc_open(pid, path,
                                        (SHORT)msg.arg1, (SHORT)msg.arg2);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case CREAT:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);
                msg.arg1  = (ARGTYPE) proc_open(pid, path,
                                O_RDWR | O_CREAT | O_TRUNC, (SHORT)msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case DUP:
            {
                INT pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_dup(pid, (int) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case CLOSE:
            {
                INT pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_close(pid, (int) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case SYNC:
            {
                INT pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_sync(pid);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case LSEEK:
            {
                INT pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_lseek(pid, (int) msg.arg1,
                                        (long) msg.arg2, (int) msg.arg3);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case READ:
            {
                INT     pid   = msg.pid;

                msg.arg1  = (ARGTYPE) proc_read(pid, (int) msg.arg1,
                                                        (INT) msg.arg2);
                msg.errno = errno; 
                reply(pid, UNCRITICAL); 
            }
            break;
        case WRITE:
            {
                INT         pid   = msg.pid;

                msg.arg1  = (ARGTYPE) proc_write(pid, (int) msg.arg1,
                                                        (INT) msg.arg2);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case LINK:
            {
                INT     pid = msg.pid;
                char    old_path[64];
                char    new_path[64];

                (void) STRCPY(old_path, msg.str1);
                (void) STRCPY(new_path, msg.str2);
                msg.arg1  = (ARGTYPE) proc_link(pid, old_path, new_path);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case UNLINK:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);
                msg.arg1  = (ARGTYPE) proc_unlink(pid, path);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case MKNOD:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);
                msg.arg1  = (ARGTYPE) proc_mknod(pid, path, (SHORT) msg.arg1,
                                                        (SHORT) msg.arg2);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case STAT:
            {
                INT         pid = msg.pid;
                char        path[64];
                struct stat stbuf;

                (void) STRCPY(path, msg.str1);
                msg.arg1  = (ARGTYPE) proc_stat(pid, path, &stbuf);
                msg.errno = errno;
                (void) MEMCPY(msg.str2, &stbuf, sizeof(struct stat));
                reply(pid, UNCRITICAL); 
            }
            break;
        case FSTAT:
            {
                INT         pid = msg.pid;
                struct stat stbuf;

                msg.arg1  = (ARGTYPE) proc_fstat(pid, (int) msg.arg1, &stbuf);
                msg.errno = errno;
                (void) MEMCPY(msg.str2, &stbuf, sizeof(struct stat));
                reply(pid, UNCRITICAL); 
            }
            break;
        case ACCESS:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);
                msg.arg1  = (ARGTYPE) proc_access(pid, path, (SHORT) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case MOUNT:
            {
                INT     pid = msg.pid;
                char    spec[64];
                char    dir[64];

                (void) STRCPY(spec, msg.str1);
                (void) STRCPY(dir,  msg.str2);
                msg.arg1  = (ARGTYPE) proc_mount(pid, spec, dir,
                                                        (int) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case UMOUNT:
            {
                INT     pid = msg.pid;
                char    spec[64];

                (void) STRCPY(spec, msg.str1);
                msg.arg1  = (ARGTYPE) proc_umount(pid, spec);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case PIPE:
            {
                INT pid = msg.pid;
                int pfd[2];

                msg.arg1  = (ARGTYPE) proc_pipe(pid, pfd);
                msg.errno = errno;
                msg.arg2  = (ARGTYPE) pfd[0];
                msg.arg3  = (ARGTYPE) pfd[1];
                reply(pid, UNCRITICAL);
            }
            break;
        case UMASK:
            {
                INT pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_umask(pid, (SHORT) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case CHMOD:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);

                msg.arg1  = (ARGTYPE) proc_chmod(pid, path, (SHORT) msg.arg1);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case CHOWN:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);

                msg.arg1  = (ARGTYPE) proc_chown(pid, path, (SHORT) msg.arg1,
                                                            (SHORT) msg.arg2);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case CHDIR:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);

                msg.arg1  = (ARGTYPE) proc_chdir(pid, path);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case CHROOT:
            {
                INT     pid = msg.pid;
                char    path[64];

                (void) STRCPY(path, msg.str1);

                msg.arg1  = (ARGTYPE) proc_chroot(pid, path);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case FCNTL:
            {
                INT     pid = msg.pid;

                msg.arg1  = (ARGTYPE) proc_fcntl(pid, (int) msg.arg1,
                                            (int) msg.arg2, (INT) msg.arg3);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break;
        case USTAT:
            {
                INT             pid = msg.pid;
                struct ustat    ubuf;

                msg.arg1  = (ARGTYPE) proc_ustat((SHORT) msg.arg1, &ubuf);
                msg.errno = errno;

                (void) MEMCPY(msg.str2, &ubuf, sizeof(struct ustat));

                reply(pid, UNCRITICAL);
            }
            break;
        case UTIME:
            {
                INT             pid = msg.pid;
                char            path[64];

                (void) STRCPY(path, msg.str1);

                msg.arg1  = (ARGTYPE) proc_utime(pid, path,
                                (long) msg.arg1, (long) msg.arg2);
                msg.errno = errno;
                reply(pid, UNCRITICAL);
            }
            break; 
        case IOCTL:
            {
                INT             pid = msg.pid;
                INT             size;
                char            arg[64];

                (void) MEMCPY(arg, msg.str1, 64);

                msg.arg1  = (ARGTYPE) proc_ioctl(pid, (int) msg.arg1,
                                                      (int) msg.arg2,
                                                      arg, &size);
                msg.errno = errno;
                msg.arg2  = (ARGTYPE) size;

                if ( size > 0 && size < 65 )
                    (void) MEMCPY(msg.str1, arg, size);

                reply(pid, UNCRITICAL);
            }
            break;
        case FORK:
            {
                INT chan = msg.chan;

                proc_fork((INT) msg.arg1, (INT) msg.arg2);
                msg.arg1  = (ARGTYPE) 0;
                msg.errno = errno;
                msg.cmd   = PMDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case SETEID:
            {
                INT chan = msg.chan;

                msg.arg1  = (ARGTYPE) proc_seteid((INT) msg.arg1,
                                        (SHORT) msg.arg2, (SHORT) msg.arg3);
                msg.errno = errno;
                msg.cmd   = PMDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break; 
        case EXEC:
            {
                INT     chan = msg.chan;
                char    path[64];
                LONG    fid;

                (void) STRCPY(path, msg.str1);

                msg.arg1  = (ARGTYPE) proc_exec(path, (INT) msg.arg1, &fid);
                msg.errno = errno;
                msg.arg2  = (ARGTYPE) fid;
                msg.cmd   = PMXDONE;
                msg.chan  = chan;

                reply(MM_PID, CRITICAL);
            }
            break;
        case XSTAT:
            {
                INT         chan = msg.chan;
                struct stat stbuf;
                char        path[64];

                (void) STRCPY(path, msg.str1);
                msg.arg1  = (ARGTYPE) proc_stat((INT) msg.arg1, path, &stbuf);
                msg.errno = errno;
                (void) MEMCPY(msg.str2, &stbuf, sizeof(struct stat));
                msg.cmd   = PMSDONE;
                msg.chan  = chan;

                reply(PM_PID, CRITICAL);
            }
            break;
        case EXIT:
            {
                INT chan = msg.chan;

                proc_exit((INT) msg.arg1);
                msg.arg1  = (ARGTYPE) 0; 
                msg.errno = errno;
                msg.cmd   = PMDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case XAHDR:
            {
                INT     chan   = msg.chan;
                AHDR    ahdr; 

                msg.arg1  = (ARGTYPE) proc_ahdr((INT) msg.arg1, &ahdr);
                msg.errno = errno;
                msg.cmd   = AHDRDONE;
                msg.chan  = chan;

                (void) MEMCPY(msg.str2, &ahdr, sizeof(AHDR));
                reply(MM_PID, CRITICAL);
            }
            break;
        case BEGDMP:
            {
                INT     chan = msg.chan;

                msg.arg1  = (ARGTYPE) proc_open((INT) msg.arg1, "core",
                                    O_WRONLY | O_CREAT | O_TRUNC, 0600);
                msg.errno = errno;
                msg.cmd   = PMDDONE;
                msg.chan  = chan;
                
                reply(PM_PID, CRITICAL);
            }
            break;
        case ENDDMP:
            {
                INT chan = msg.chan;

                msg.arg1  = (ARGTYPE) proc_close((INT) msg.arg1,
                                                        (int) msg.arg2);
                msg.errno = errno;
                msg.cmd   = PMDONE;
                msg.chan  = chan;
                reply(PM_PID, CRITICAL);
            }
            break;
        case GETBLKS:
            {
                INT     frame = (INT) msg.arg1;
                INT     chan  = msg.chan;
                BLKNO   blks[4];

                blks[0] = (BLKNO) msg.arg3;
                blks[1] = (BLKNO) msg.arg4;
                blks[2] = (BLKNO) msg.arg5;
                blks[3] = (BLKNO) msg.arg6;

                msg.arg1  = (ARGTYPE) proc_getblks((SHORT) msg.arg2,
                                                            blks, chan);
                msg.errno = errno;
                msg.cmd   = BLKSDONE;
                msg.arg2  = (ARGTYPE) frame;
                msg.chan  = chan;
                reply(MM_PID, CRITICAL);            
            }
            break;
        case PUTBLKS:
            {
                INT     chan = msg.chan;

                msg.arg1  = (ARGTYPE) proc_putblks((INT) msg.arg1,
                                                    (int) msg.arg2, chan);
                msg.errno = errno;
                msg.cmd   = DUMPDONE;
                msg.chan  = chan;
                reply(MM_PID, CRITICAL);            
            }
            break;
        case BLKNOS:
            {
                INT chan = msg.chan;

                msg.arg1  = (ARGTYPE) proc_blknos((LONG) msg.arg1,
                                                    (BLKNO) msg.arg2,
                                                    (INT) msg.arg3, chan);
                msg.errno = errno;
                msg.cmd   = BLKNODONE; 
                msg.chan  = chan;
                reply(MM_PID, CRITICAL);
            }           
            break;
        case OPEN_DONE:
            dev_op_done(msg.chan, msg.errno);
            break;
        case READ_DONE:
            dev_rd_done(msg.chan, (int)msg.arg1, (DEV)msg.arg2, msg.errno);
            break;
        case WRITE_DONE:
            dev_wr_done(msg.chan, (int)msg.arg1, msg.errno);
            break;
        case IOCTL_DONE:
            dev_ioctl_done(msg.chan, msg.str1, (int)msg.arg1, msg.errno);
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
    perror("fm"); 
 
    exit(1);
}

/*----------------------------------------------------*/

