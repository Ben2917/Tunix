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
* These stubs communicate between a user process and the
* system processes via the kernel.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/fcntl.h"
#include    "../include/page.h"
#include    "../include/ipc.h"
#include    "../include/msg.h"
#include    "../include/sem.h"
#include    "../include/shm.h"
#include    "../include/comm.h"
#include    "../include/signal.h"
#include    "../include/times.h"
#include    "../include/ustat.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/fcodes.h"
#include    "../include/kmsg.h"
#include    "../include/pid.h"
#include    "../include/fault.h"

#define NFILES      20

extern int  errno;
extern int  debug;

extern void pause_request(); 

int     my_pid = INIT_PID; 
int     fdl    = -1;

static MSG          msg;              
static int          msg_i;

#define BLK_SIZE    4096

static char logname[16];
static HDLR sighdlr[NSIG];  /* handlers for the signals             */ 

static void request();
static void copy_in();
static void copy_out();
static void sig_catch();

#define ERROR(errnr)    { errno = (errnr); return -1; }

/*-----------------------------------------------*/

void s_sync()

{
    msg.cmd = SYNC;

    request(FM_PID);

    errno = msg.errno;
}
 
/*-----------------------------------------------*/

int s_open(path, mode, perms)

char    *path;
int     mode;
int     perms;

{
    msg.cmd  = OPEN;
    msg.arg1 = (ARGTYPE) mode;
    msg.arg2 = (ARGTYPE) perms;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_creat(path, perms)

char    *path;
int     perms;

{
    msg.cmd  = CREAT;
    msg.arg1 = (ARGTYPE) perms;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_dup(fd)

int fd;

{
    msg.cmd  = DUP;
    msg.arg1 = (ARGTYPE) fd;

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_close(fd)

int fd;

{
    msg.cmd  = CLOSE;
    msg.arg1 = (ARGTYPE) fd;

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

long s_lseek(fd, offset, dir)

int     fd;
long    offset;
int     dir;

{
    msg.cmd  = LSEEK;
    msg.arg1 = (ARGTYPE) fd;
    msg.arg2 = (ARGTYPE) offset; 
    msg.arg3 = (ARGTYPE) dir;

    request(FM_PID);

    errno = msg.errno;

    return (long) msg.arg1;
}

/*-----------------------------------------------*/

int s_read(fd, buf, nbytes)

int     fd;
char    *buf;
INT     nbytes;

{
    INT now;
    INT done = 0;

    while ( done < nbytes )
    {
        now = MIN(nbytes - done, DATA_SIZE);

        msg.cmd  = READ;
        msg.arg1 = (ARGTYPE) fd;
        msg.arg2 = (ARGTYPE) now;
        msg.chan = STD_CHAN;

        request(FM_PID);

        errno = msg.errno;

        if ( errno || (int) msg.arg1 <= 0 )
            break;
        else
            copy_in(FM_PID, buf, (int) msg.arg1);

        buf  += (int) msg.arg1;
        done += (INT) msg.arg1;

        if ( (int) msg.arg1 < now )
            break;
    }

    return (int) done;
}

/*-----------------------------------------------*/

int s_write(fd, buf, nbytes)

int     fd;
char    *buf;
INT     nbytes;

{
    INT     now;
    INT     done = 0;

    while ( done < nbytes )
    {
        now = MIN(nbytes - done, DATA_SIZE);

        copy_out(FM_PID, buf, now);

        msg.cmd  = WRITE;
        msg.arg1 = (ARGTYPE) fd;
        msg.arg2 = (ARGTYPE) now;
        msg.chan = STD_CHAN;

        request(FM_PID);

        errno = msg.errno;

        if ( errno || (int) msg.arg1 != now )
            break;

        buf  += now;
        done += now;
    }

    return (int) done;
}

/*-----------------------------------------------*/

int s_link(old_path, new_path)

char    *old_path;
char    *new_path;

{
    msg.cmd   = LINK;

    (void) STRCPY(msg.str1, old_path);
    (void) STRCPY(msg.str2, new_path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_unlink(path)

char    *path;

{
    msg.cmd   = UNLINK;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_mknod(path, perms, dev)

char    *path;
int     perms;
int     dev;

{
    msg.cmd  = MKNOD;
    msg.arg1 = (ARGTYPE) perms;
    msg.arg2 = (ARGTYPE) dev;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_stat(path, stbuf)

char        *path;
struct stat *stbuf;

{
    msg.cmd = STAT;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    if ( errno == 0 )
        (void) MEMCPY(stbuf, msg.str2, sizeof(struct stat)); 

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_fstat(fd, stbuf)

int         fd;
struct stat *stbuf;

{
    msg.cmd  = FSTAT;
    msg.arg1 = (ARGTYPE) fd;

    request(FM_PID);

    errno = msg.errno;

    if ( errno == 0 )
        (void) MEMCPY(stbuf, msg.str2, sizeof(struct stat));

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_access(path, amode)

char    *path;
int     amode;

{
    msg.cmd  = ACCESS;
    msg.arg1 = (ARGTYPE) amode;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_mount(spec, dir, rwflag)

char    *spec;
char    *dir;
int     rwflag;

{
    msg.cmd  = MOUNT;
    msg.arg1 = (ARGTYPE) rwflag;

    (void) STRCPY(msg.str1, spec);
    (void) STRCPY(msg.str2, dir);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_umount(spec)

char    *spec;

{
    msg.cmd = UMOUNT;

    (void) STRCPY(msg.str1, spec);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_pipe(pfd)

int *pfd;

{
    msg.cmd = PIPE;

    request(FM_PID);

    errno = msg.errno;

    pfd[0] = (int) msg.arg2;
    pfd[1] = (int) msg.arg3;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_umask(new_mask)

int new_mask;

{
    msg.cmd  = UMASK;
    msg.arg1 = (ARGTYPE) new_mask;

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_chmod(path, mode)

char    *path;
int     mode;

{
    msg.cmd  = CHMOD;
    msg.arg1 = (ARGTYPE) mode;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_chown(path, owner, group)

char    *path;
int     owner;
int     group;

{
    msg.cmd  = CHOWN;
    msg.arg1 = (ARGTYPE) owner;
    msg.arg2 = (ARGTYPE) group;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_chdir(path)

char    *path;

{
    msg.cmd = CHDIR;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_chroot(path)

char    *path;

{
    msg.cmd = CHROOT;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_fcntl(fd, cmd, arg)

int fd;
int cmd;
int arg;

{
    msg.cmd  = FCNTL;
    msg.arg1 = (ARGTYPE) fd;
    msg.arg2 = (ARGTYPE) cmd;
    msg.arg3 = (ARGTYPE) arg;

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_ustat(dev, ubufp)

DEV             dev;
struct ustat    *ubufp;

{
    msg.cmd  = USTAT;
    msg.arg1 = (ARGTYPE) dev;

    request(FM_PID);

    (void) MEMCPY(ubufp, msg.str2, sizeof(struct ustat));

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_utime(path, axtime, modtime)

char    *path;
LONG    axtime;
LONG    modtime;

{
    msg.cmd  = UTIME;
    msg.arg1 = (ARGTYPE) axtime;
    msg.arg2 = (ARGTYPE) modtime;

    (void) STRCPY(msg.str1, path);

    request(FM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_ioctl(fd, req, arg)

int     fd;
int     req;
char    *arg;

{
    int     size;

    msg.cmd  = IOCTL;
    msg.arg1 = (ARGTYPE) fd;
    msg.arg2 = (ARGTYPE) req;

    (void) MEMCPY(msg.str1, arg, 64);

    request(FM_PID);

    errno = msg.errno;

    size = (int) msg.arg2;

    if ( size > 0 && size < 65 )
        (void) MEMCPY(arg, msg.str1, size);

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_fork()

{
    msg.cmd = FORK; 

    request(PM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_exec(path)

char    *path;

{
    msg.cmd = EXEC;

    (void) STRCPY(msg.str1, path);

    request(PM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

void s_exit(xval)

INT xval;

{
    msg.cmd  = EXIT;
    msg.arg1 = (ARGTYPE) xval;

    send(PM_PID, &msg, UNCRITICAL);

    msg.cmd   = LISTEN;
    send(0, &msg, KER_MSG);
}

/*-----------------------------------------------*/

int s_pause()

{
    msg.cmd = PAUSE;

    pause_request(PM_PID, &msg);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_wait(statp)

INT     *statp;

{
    msg.cmd = WAIT;

    request(PM_PID);

    errno  = msg.errno;
    *statp = (INT) msg.arg2;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

void s_getpid(ppidp, pgidp)

INT     *ppidp, *pgidp;

{
    msg.cmd = GETPPID;

    request(PM_PID);

    errno  = msg.errno;
    *ppidp = (INT) msg.arg2;
    *pgidp = (INT) msg.arg3;
}

/*-----------------------------------------------*/

void s_getuid(uidp, gidp, euidp, egidp)

INT     *uidp;
INT     *gidp;
INT     *euidp;
INT     *egidp;

{
    msg.cmd = GETUID;

    request(PM_PID);

    errno  = msg.errno;
    *uidp  = (INT) msg.arg2;
    *gidp  = (INT) msg.arg3;
    *euidp = (INT) msg.arg4;
    *egidp = (INT) msg.arg5;
}

/*-----------------------------------------------*/

int s_nice(incr)

INT     incr;

{
    msg.cmd  = NICE; 
    msg.arg1 = (ARGTYPE) incr;

    request(PM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_setpgrp()

{
    msg.cmd = SETPGRP;

    request(PM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_setuid(new_uid, new_gid)

SHORT   new_uid;
SHORT   new_gid;

{
    msg.cmd  = SETUID;
    msg.arg1 = (ARGTYPE) new_uid;
    msg.arg2 = (ARGTYPE) new_gid;

    request(PM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

void s_times(tmsp)

struct tms  *tmsp;

{
    msg.cmd = TIMES;

    request(PM_PID);

    errno = msg.errno;

    (void) MEMCPY(tmsp, msg.str1, sizeof(struct tms));
}

/*-----------------------------------------------*/

int s_kill(kpid, sig)

int     kpid;
INT     sig;

{
    msg.cmd  = KILL;
    msg.arg1 = (ARGTYPE) kpid;
    msg.arg2 = (ARGTYPE) sig;

    request(PM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

HDLR s_signal(sig, hdlr)

int     sig;
HDLR    hdlr;

{
    HDLR    old_hdlr = sighdlr[sig];

    sighdlr[sig] = hdlr;

    msg.cmd  = SIGNAL;
    msg.arg1 = (ARGTYPE) sig;

    if ( hdlr != SIG_DFL && hdlr != SIG_IGN )
        msg.arg2 = (ARGTYPE) SIG_HDL;
    else
        msg.arg2 = (ARGTYPE) hdlr;

    request(PM_PID);

    errno = msg.errno;

    return ( errno ) ? (HDLR)-1 : old_hdlr;
}

/*-----------------------------------------------*/

INT s_alarm(sec)

INT     sec;

{
    msg.cmd  = ALARM;
    msg.arg1 = (ARGTYPE) sec;

    request(PM_PID);

    errno = msg.errno;

    return (INT) msg.arg1;
}

/*-----------------------------------------------*/

int s_brk(new_data_end)

ADDR    new_data_end;

{
    msg.cmd  = BRK;
    msg.arg1 = (ARGTYPE) new_data_end;

    request(PM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_msgget(key, msgflg)

key_t   key;
int     msgflg;

{
    msg.cmd  = MSGGET;
    msg.arg1 = (ARGTYPE) msgflg;
    msg.arg2 = (ARGTYPE) key;
 
    request(IM_PID);

    errno = msg.errno;

    return (int) msg.arg1; 
}

/*-----------------------------------------------*/

int s_msgsnd(msqid, msgp, msgsz, msgflg)

int      msqid;
MSGBUF   *msgp;
int      msgsz;
int      msgflg;

{
    msg.cmd  = MSGSND;
    msg.arg1 = (ARGTYPE) msqid;
    msg.arg2 = (ARGTYPE) msgp->mtype;
    msg.arg3 = (ARGTYPE) msgsz;
    msg.arg4 = (ARGTYPE) msgflg;
    msg.chan = STD_CHAN; 

    copy_out(IM_PID, msgp->mtext, msgsz);

    request(IM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_msgrcv(msqid, msgp, msgsz, msgtyp, msgflg)

int      msqid;
MSGBUF   *msgp;
int      msgsz;
long     msgtyp;
int      msgflg;

{
    msg.cmd  = MSGRCV;
    msg.arg1 = (ARGTYPE) msqid;
    msg.arg2 = (ARGTYPE) msgtyp;
    msg.arg3 = (ARGTYPE) msgsz;
    msg.arg4 = (ARGTYPE) msgflg;
    msg.chan = STD_CHAN;

    request(IM_PID);

    errno = msg.errno;

    if ( errno == 0 )
    {
        msgp->mtype = (LONG) msg.arg1;

        copy_in(IM_PID, msgp->mtext, msgsz);
    }

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_msgctl(msqid, cmd, buf)

int    msqid;
int    cmd;
MSGID  *buf;

{
    msg.cmd  = MSGCTL;
    msg.arg1 = (ARGTYPE) msqid;
    msg.arg2 = (ARGTYPE) cmd;

    (void) MEMCPY(msg.str1, buf, sizeof(MSGID));

    request(IM_PID);

    (void) MEMCPY(buf, msg.str1, sizeof(MSGID));

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

typedef union
{
	int	   val;
	SEMID  *buf;
	SHORT  *array;

} ARG_TYPE;

int s_semctl(semid, semnum, cmd, arg)

int      semid;
int      semnum;
int      cmd;
ARG_TYPE arg;

{
    msg.cmd  = SEMCTL;
    msg.arg1 = (ARGTYPE) semid;
    msg.arg2 = (ARGTYPE) cmd;
    msg.arg3 = (ARGTYPE) semnum;

    switch ( cmd )
    {
    case IPC_SET:
        (void) MEMCPY(msg.str1, arg.buf, sizeof(SEMID));
        break;
    case SETALL:
        (void) MEMCPY(msg.str1, arg.array, 10 * sizeof(SHORT));
        break;
    default:
        msg.arg4 = (ARGTYPE) arg.val;
    }

    request(IM_PID);

    switch ( cmd )
    {
    case IPC_STAT:
        (void) MEMCPY(arg.buf, msg.str1, sizeof(SEMID));
        break;
    case GETALL:
        (void) MEMCPY(arg.array, msg.str1, 10 * sizeof(SHORT));
        break;
    }

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_semget(key, nsems, semflg)

key_t   key;
int     nsems;
int     semflg;

{
    msg.cmd  = SEMGET;
    msg.arg1 = (ARGTYPE) semflg;
    msg.arg2 = (ARGTYPE) nsems;
    msg.arg3 = (ARGTYPE) key;

    request(IM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_semop(semid, sops, nsops)

int     semid;
SEMBUF  **sops;
int     nsops;

{
    msg.cmd  = SEMOP;
    msg.arg1 = (ARGTYPE) semid;
    msg.arg2 = (ARGTYPE) nsops;

    (void) MEMCPY(msg.str1, *sops, 10 * sizeof(SEMBUF));

    request(IM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_shmctl(shmid, cmd, buf)

int   shmid;
int   cmd;
SHMID *buf;

{
    msg.cmd  = SHMCTL;
    msg.arg1 = (ARGTYPE) shmid;
    msg.arg2 = (ARGTYPE) cmd;

    (void) MEMCPY(msg.str1, buf, sizeof(SHMID));

    request(IM_PID);

    (void) MEMCPY(buf, msg.str1, sizeof(SHMID));

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

int s_shmget(key, size, shmflg)

key_t   key;
int     size;
int     shmflg;

{
    msg.cmd  = SHMGET;
    msg.arg1 = (ARGTYPE) key;
    msg.arg2 = (ARGTYPE) size;
    msg.arg3 = (ARGTYPE) shmflg;

    request(IM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

char *s_shmat(shmid, addr, shmflg)

int     shmid;
char    *addr;
int     shmflg;

{
    msg.cmd  = SHMAT;
    msg.arg1 = (ARGTYPE) shmid;
    msg.arg2 = (ARGTYPE) addr;
    msg.arg3 = (ARGTYPE) shmflg;

    request(IM_PID);

    errno = msg.errno;

    return (char *) msg.arg1;
}

/*-----------------------------------------------*/

int s_shmdt(addr)

char    *addr;

{
    msg.cmd  = SHMDT;
    msg.arg1 = (ARGTYPE) addr;

    request(IM_PID);

    errno = msg.errno;

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

void s_init()

{
    int     i;

    (void) signal(SIGUSR2, sig_catch);

    for ( i = 0; i < NSIG; ++i )
        sighdlr[i] = SIG_DFL;

    printf("pid = %d\n", my_pid);
  
    if ( debug )
    {
        (void) sprintf(logname, "log%d", my_pid);
        fdl = open(logname, O_WRONLY | O_CREAT | O_TRUNC, 0700);
    }

    comm_init();
    msg_i = begin_msg((key_t) MSGKEY_I);

    msg.cmd  = INIT;
    msg.arg1 = (ARGTYPE) getpid();
    send(0, &msg, UNCRITICAL);

    (void) receive(&msg, FALSE);

    if ( errno != 0 && errno != EINTR )
        perror("user: s_init");
}

/*-----------------------------------------------*/

static void request(tno)

INT     tno;

{
    send(tno, &msg, UNCRITICAL);
    receive(&msg, FALSE);

    if ( errno != 0 && errno != EINTR )
        perror("user: request");
}

/*-----------------------------------------------*/

static void copy_in(pid, buf, nbytes)

char    *buf;
INT     nbytes;

{
    data_in(pid, STD_CHAN, buf, nbytes);
}

/*-----------------------------------------------*/

static void copy_out(pid, buf, nbytes)

INT     pid;
char    *buf;
INT     nbytes;

{
    data_out(pid, STD_CHAN, buf, nbytes);
}

/*-----------------------------------------------*/

#define IMSGSZ  sizeof(IMSG) - sizeof(long)

void sig_catch()

{
    IMSG    imsg; 
    HDLR    h; 
 
    (void) signal(SIGUSR2, sig_catch);

    (void) msgrcv(msg_i, (MSGBUF *)&imsg, IMSGSZ, (long)my_pid, 0);

    h = sighdlr[imsg.int_no];

    if ( h != SIG_IGN && h != SIG_DFL )
        (*h)();

    sighdlr[imsg.int_no] = SIG_DFL;
}
