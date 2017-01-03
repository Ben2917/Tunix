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
* This module is responsible for the representation of
* abstract processes. Here is where the process table is
* managed and the process identifiers (pid) are handed out.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/errno.h"
#include    "../include/fcntl.h"
#include    "../include/times.h"
#include    "../include/stat.h"
#include    "../include/signal.h"
#include    "../include/callout.h"
#include    "../include/chan.h"
#include    "../include/fmcall.h"
#include    "../include/mmcall.h"
#include    "../include/ipcmcall.h"
#include    "../include/kcall.h"
#include    "../include/proc.h"
#include    "../include/pid.h"
#include    "../include/macros.h"

extern int errno;

extern void notify();

#define ERROR(eno)  { errno = eno; return -1; }

#define N_PROCS     128         /* size of process table                */
#define N_PRI       20          /* number of priority levels            */
#define BASE_PRI    2           /* boundary to kernel priorities        */
#define N_PID       (MAX_PID/(8*sizeof(INT)))   /* no. of distinct pids */
#define HZ          60          /* no. of ticks per second              */

/************************************************************************/
/*                                                                      */
/*  The following structure contains all the information about a pro-   */
/*  cess that is needed by the process manager (other information is    */
/*  stored in the file manager for example). This structure contains a  */
/*  lot of information that strictly speaking wouldn't necessarily have */
/*  to be in the kernel memory at all times -- such as for example the  */
/*  size or the handlers for signals. In traditional UNIX implementa-   */
/*  tions such information is stored in the "u area" and is thus        */
/*  swapable. Maybe in a later implementation we'll do something        */
/*  similar about the data stored for each process.                     */
/*                                                                      */
/************************************************************************/

typedef struct proc
{
    INT         pid;            /* process identifier of this process   */
    INT         ppid;           /*    "         "     "    "  parent    */
    INT         pgid;           /* process group identifier             */
    SHORT       uid;            /* user id of owner of process          */
    SHORT       gid;            /* group id of  "   "    "              */
    SHORT       euid;           /* effective user id of process         */
    SHORT       egid;           /* effective group id of  "             */
    SHORT       suid;           /* saved uid (after setuid)             */
    SHORT       sgid;           /*  "    gid    "     "                 */
    SHORT       pri;            /* priority level of process            */
    SHORT       nice;           /* nice value set by sys. call nice     */
    SHORT       state;          /* see below for possible states        */ 
    SHORT       pause;          /* is process waiting in pause?         */
    INT         cpu;            /* recent cpu usage of this process     */
    INT         xstat;          /* exit status                          */
    INT         sigs;           /* bits for received signals            */   
    INT         active;         /* no. of children still active         */
    LONG        utime;          /* cpu time (in ticks) used so far      */
    LONG        ctime;          /* cpu time (in ticks) used by children */
    HDLR        sighdlr[NSIG];  /* handlers for the signals             */  
    struct proc *next;          /* used in linked lists of ready/sleep  */
    struct proc *prev;          /* chains                               */

} PROC;

/* The states a process can have    */

#define INIT    0           /* just being loaded (exec)         */
#define READY   1           /* ready to run but preempted       */
#define RUNNING 2           /* the currently running process    */
#define SLEEP   3           /* waiting for a message            */
#define EXITING 4           /* being cleaned up                 */
#define ZOMBIE  5           /* dead but not yet removed         */

#define NIL_PROC    (PROC *)0

static PROC     procs[N_PROCS];
static PROC     *running;   /* the currently running process    */

typedef struct
{
    PROC    *first;
    PROC    *last;

} PLIST;

static PLIST    rlist[N_PRI];   /* lists of ready processes     */
static PLIST    wlist;          /* list of waiting processes    */

static INT      pids[N_PID];
static INT      next_pid;
static INT      tick_count;

/*----------- Declarations of local functions -------------------------*/

static PROC *find_proc();
static void append_ready();
static PROC *next_ready();
static void put_in();
static void take_out();
static void wake_up();
static void abort();
static int  kill_one();
static int  schedule();
static void recompute();
static int  dead_child();
static INT  issig();
static int  core_dump();
static INT  new_pid();
static void release_pid();

/*----------- Definitions of public functions -------------------------*/

void proc_init()

{
    PROC    *pp;
    PLIST   *pl;
    INT     i;

    chan_init();
    call_init();

    next_pid = 0;

    for ( pp = procs; pp < procs + N_PROCS; ++pp )
        pp->pid = 0;

    for ( pl = rlist; pl < rlist + N_PRI; ++pl )
        pl->first = pl->last = NIL_PROC;

    wlist.first = NIL_PROC;
    wlist.last  = NIL_PROC;

    for ( i = 0; i < N_PID; ++ i )
        pids[i] = 0;

    pp = procs;

    /* create process init  */

    pp->pid   = INIT_PID;
    pp->ppid  = INIT_PID;
    pp->pgid  = INIT_PID;
    pp->uid   = ROOT_UID;
    pp->gid   = ROOT_GID;
    pp->euid  = ROOT_UID;
    pp->egid  = ROOT_GID;
    pp->cpu   = 0;
    pp->sigs  = 0;
    pp->utime = 0L;
    pp->ctime = 0L;
    pp->pri   = BASE_PRI;
    pp->nice  = 0;
    pp->state = RUNNING;
    pp->pause = FALSE;

    for ( i = 0; i < NSIG; ++i ) 
        pp->sighdlr[i] = SIG_DFL; 
 
    running    = pp;
    tick_count = 0;
}

/*---------------------------------------------------------------*/

int proc_fork(pid)

INT     pid;

{
    PROC    *pp;
    PROC    *ppp = find_proc(pid);

    for ( pp = procs; pp < procs + N_PROCS; ++pp )
        if ( pp->pid == 0 )
            break;

    if ( pp >= procs + N_PROCS )
        ERROR(EAGAIN)

    (void) MEMCPY(pp, ppp, sizeof(PROC)); 
 
    pp->pid   = new_pid();
    pp->ppid  = ppp->pid;
    pp->state = INIT;
    pp->pause = FALSE;
    pp->cpu   = 0; 
    pp->utime = 0L; 
    pp->ctime = 0L; 
    pp->pri   = BASE_PRI; 

    if ( mm_fork(pp->pid, pp->ppid)  == -1 ||
         fm_fork(pp->pid, pp->ppid)  == -1 ||
         ipc_fork(pp->pid, pp->ppid) == -1 )
        return -1;

    pp->state = READY;
    append_ready(pp);

    return (int) pp->pid;
}

/*---------------------------------------------------------------*/

int proc_exec(pid, path)

INT     pid;
char    *path;

{
    PROC        *pp = find_proc(pid);
    INT         pno = (pp - procs);
    struct stat stbuf;

    if ( fm_xstat(pp->pid, path, &stbuf) == -1 ) 
        return -1; 
 
    if ( stbuf.tx_mode & ISUID )
    {
        pp->suid = pp->euid;
        pp->euid = stbuf.tx_uid;
    }
    else
        pp->suid = pp->uid;

    if ( stbuf.tx_mode & ISGID )
    {
        pp->sgid = pp->egid;
        pp->egid = stbuf.tx_gid;
    }
    else
        pp->sgid = pp->gid;

    return mm_exec(pp->pid, path);
}

/*---------------------------------------------------------------*/

void proc_exit(pid, xstat)

INT     pid;
INT     xstat;

{
    abort(pid, 256 * xstat, FALSE);
}

/*---------------------------------------------------------------*/

int proc_pause(pid)

INT     pid;

{
    PROC    *pp = find_proc(pid);
    INT     sig;

    if ( (sig = issig(pp)) != 0 && pp->sighdlr[sig] != SIG_IGN )
        ERROR(EINTR)

    pp->pause = TRUE;

    return 0;
}

/*---------------------------------------------------------------*/

int proc_wait(pid, statp)

INT     pid;
INT     *statp;

{
    int     ignore;
    PROC    *pp = find_proc(pid);
    PROC    *cpp;
    INT     cpid;

    ignore = ( pp->sighdlr[SIGCLD] == SIG_IGN );

    for ( cpp = procs, pp->active = 0; cpp < procs + N_PROCS; ++cpp )
        if ( cpp->pid != 0 && cpp->ppid == pid && cpp->pid != pid )
        {
            if ( cpp->state == ZOMBIE )
            {
                pp->ctime += cpp->utime + cpp->ctime;
                *statp     = cpp->xstat;
                cpid       = cpp->pid;

                release_pid(cpid); 
                cpp->pid = 0;
 
                if ( !ignore )
                    return (int) cpid;
            }
            else
                ++pp->active;
        }

    if ( pp->active == 0 )
        ERROR(ECHILD)

    put_in(&wlist, pp);

    return 0;
}

/*---------------------------------------------------------------*/

void proc_getpid(pid, ppidp, pgidp)

INT     pid;
INT     *ppidp;
INT     *pgidp;

{
    PROC    *pp = find_proc(pid);

    *ppidp = pp->ppid;
    *pgidp = pp->pgid;
}

/*---------------------------------------------------------------*/

void proc_getuid(pid, uidp, gidp, euidp, egidp)

INT     pid;
SHORT   *uidp;
SHORT   *gidp;
SHORT   *euidp;
SHORT   *egidp;

{
    PROC    *pp = find_proc(pid);

    *uidp  = pp->uid;
    *gidp  = pp->gid;
    *euidp = pp->euid;
    *egidp = pp->egid;
}

/*---------------------------------------------------------------*/

int proc_nice(pid, incr)

INT     pid;
INT     incr;

{
    PROC    *pp = find_proc(pid);

    pp->nice += incr;

    if ( pp->nice >= (SHORT) (N_PRI - BASE_PRI) )
        pp->nice = N_PRI - BASE_PRI - 1;

    return (int) pp->nice;
}

/*---------------------------------------------------------------*/

int proc_setpgrp(pid)

INT     pid;

{
    PROC    *pp = find_proc(pid);

    pp->pgid = pid;

    return (int) pid;
}

/*---------------------------------------------------------------*/

int proc_setuid(pid, new_uid, new_gid)

INT     pid;
SHORT   new_uid;
SHORT   new_gid;

{
    PROC    *pp = find_proc(pid);

    if ( pp->euid == ROOT_UID )
    {
        pp->uid = pp->euid = new_uid;
        pp->gid = pp->egid = new_gid;
    }
    else if ( pp->uid == new_uid || pp->suid == new_uid )
    {
        pp->euid = new_uid;
        pp->egid = new_gid;
    }
    else if ( pp->gid == new_gid || pp->sgid == new_gid )
        pp->egid = new_gid;
    else
        ERROR(EPERM)

    if ( fm_setuid(pid, new_uid, new_gid)  == -1 ||
         ipc_setuid(pid, new_uid, new_gid) == -1 )
        return -1;

    return 0;
}

/*---------------------------------------------------------------*/

void proc_times(pid, tp)

INT         pid;
struct tms  *tp;

{
    PROC    *pp = find_proc(pid);

    tp->tms_utime  = pp->utime;
    tp->tms_stime  = 0L;            /* don't know how to compute this   */
    tp->tms_cutime = pp->ctime;
    tp->tms_cstime = 0L;
}

/*---------------------------------------------------------------*/

int proc_kill(pid, kpid, sig)

INT     pid;
int     kpid;
INT     sig;

{
    PROC    *pp  = find_proc(pid);
    SHORT   euid = pp->euid;
    PROC    *kpp; 

    if ( sig == 0 || sig >= NSIG )
        ERROR(EINVAL)

    if ( kpid > 0 )
        return kill_one(pp, (INT)kpid, sig);

    if ( kpid == 0 )
    { 
        for ( kpp = procs; kpp < procs + N_PROCS; ++kpp )
            if ( kpp->pgid == pp->pgid && kpp->pid >= BASE_PID )
                (void) kill_one(pp, kpp->pid, sig);

        return 0;
    }

    if ( pid == -1 )
    { 
        for ( kpp = procs; kpp < procs + N_PROCS; ++kpp )
            if ( (euid == ROOT_UID || kpp->uid == euid) &&
                                                kpp->pid >= BASE_PID )
                (void) kill_one(pp, kpp->pid, sig);

        return 0;
    }

    /* if we get here, kpid < -1 */

    kpid = -kpid;

    for ( kpp = procs; kpp < procs + N_PROCS; ++kpp )
        if ( kpp->pgid == (INT) kpid )
            (void) kill_one(pp, kpp->pid, sig);

    return 0;
}

/*---------------------------------------------------------------*/

HDLR    proc_signal(pid, sig, hdlr)

INT     pid;
int     sig;
HDLR    hdlr;

{
    PROC    *pp = find_proc(pid);
    HDLR    ret;

    if ( sig <= 0 || sig >= NSIG || sig == SIGKILL )
    {
        errno = EINVAL;
        return (HDLR)(-1);
    }

    ret = pp->sighdlr[sig];

    pp->sighdlr[sig] = hdlr;

    return ret;
}

/*---------------------------------------------------------------*/

INT proc_alarm(pid, sec)

INT     pid;
INT     sec;

{
    int old = call_out(pid, sec * HZ);

    return ( old >= 0 ) ? (old / HZ) : 0;
}
    
/*---------------------------------------------------------------*/

int proc_brk(pid, new_end_data)

INT     pid;
ADDR    new_end_data;

{
    return mm_brk(pid, new_end_data);
}

/*---------------------------------------------------------------*/

/*
    This function may not generate any messages!!!
*/

int proc_sleep(sigp, ticks) 
 
int     *sigp;
INT     ticks;

{
    PROC    *ip = find_proc(INIT_PID);
    int     new_pid, pid;

    tick_count += ticks; 
 
    call_tick(ticks);

    while ( (pid = call_next()) != 0 )
        (void) kill_one(ip, pid, SIGALRM);

    if ( running != NIL_PROC && running->state == RUNNING )
                            /* it could be ZOMBIE   */
    {
        running->utime += tick_count;
        running->cpu   += tick_count;
        running->state  = SLEEP;
    }

    recompute();

    new_pid = schedule();

    *sigp = ( running != NIL_PROC) ? (int) running->sigs : 0;

    return new_pid;
}   

/*---------------------------------------------------------------*/

/*
    This function may not generate any messages!!!
*/

int proc_wakeup(pid, sigp, ticks)

INT     pid;
int     *sigp;
INT     ticks;

{
    PROC    *pp = find_proc(pid);
    PROC    *ip = find_proc(INIT_PID);
    int     new_pid, kpid;

    *sigp = 0;                      /* just to be sure          */

    tick_count += ticks; 
 
    call_tick(ticks);

    while ( (kpid = call_next()) != 0 )
        (void) kill_one(ip, kpid, SIGALRM);

    if ( pp == running )
        return (int) pid;           /* just to be quite sure    */

    wake_up(pp);

    if ( running != NIL_PROC && running->state == RUNNING )
    {                        /* it could be zombie  */
        running->utime += tick_count;
        running->cpu   += tick_count;
        running->state  = READY;
        append_ready(running);
    }
 
    recompute();

    new_pid = schedule();

    *sigp = ( running != NIL_PROC ) ? (int) running->sigs : 0;

    return new_pid;
}

/*---------------------------------------------------------------*/

/*
    This function may not generate any messages!!!
*/

int proc_tick(sigp, ticks)

int     *sigp;
INT     ticks;

{
    PROC    *ip = find_proc(INIT_PID);
    INT     pid;
    int     new_pid;

    tick_count += ticks; 
 
    call_tick(ticks);

    while ( (pid = call_next()) != 0 )
        (void) kill_one(ip, pid, SIGALRM);

    if ( running != NIL_PROC && running->state == RUNNING )
    {
        running->utime += tick_count;
        running->cpu   += tick_count;
        running->state  = READY;
        append_ready(running);
    }

    recompute(); 
 
    new_pid = schedule();

    *sigp = ( running != NIL_PROC ) ? (int) running->sigs : 0;

    return new_pid;
}

/*---------------------------------------------------------------*/

void proc_act_on_sig(pid)

INT     pid;

{
    PROC    *pp = find_proc(pid);
    INT     sig;
    HDLR    hdlr;

    while ( (sig = issig(pp)) != 0 )
    {
        hdlr = pp->sighdlr[sig];

        if ( hdlr == SIG_IGN )
            continue;

        if ( hdlr != SIG_DFL )
        {
            pp->sighdlr[sig] = SIG_DFL;

            wake_up(pp);
            call_handler(pid, sig);
        
            if ( pp->pause )
            {
                pp->pause = FALSE;
                notify(pp->pid, EINTR, 0, 0);
            }

            continue;
        }

        if ( sig == SIGCLD || sig == SIGPWR )
            continue;

        abort(pid, sig, core_dump(sig));
        break;
    }
}

/*---------------------------------------------------------------*/

void proc_fdone(chan, res, error)

INT chan;
int res;
int error;

{
    fm_finish(chan, res, error);
}

/*---------------------------------------------------------------*/

void proc_sdone(chan, cp, error)

INT     chan;
char    *cp;
int     error;

{
    fm_sfinish(chan, cp, error);
}

/*---------------------------------------------------------------*/

void proc_idone(chan, res, error)

INT chan;
int res;
int error;

{
    ipc_finish(chan, res, error);
}

/*---------------------------------------------------------------*/

void proc_mdone(chan, res, error)

INT chan;
int res;
int error;

{
    mm_done(chan, res, error);
}

/*------------------ Definitions of local functions ----------------*/

static PROC *find_proc(pid)

INT     pid;

{
    PROC    *pp;

    for ( pp = procs; pp < procs + N_PROCS; ++pp )
        if ( pp->pid == pid )
            return pp;

    return NIL_PROC;
}

/*---------------------------------------------------------------*/

static void append_ready(pp)

PROC    *pp;

{
    PLIST   *pl = &rlist[pp->pri];

    put_in(pl, pp);
}

/*---------------------------------------------------------------*/

static PROC *next_ready()

{
    PLIST   *pl;
    PROC    *pp;

    for ( pl = rlist; pl < rlist + N_PRI; ++pl )
    {
        if ( pl->first == NIL_PROC )
            continue;

        pp = pl->first;

        take_out(pl, pp);

        return pp;
    }

    return NIL_PROC;
}

/*---------------------------------------------------------------*/

static void put_in(pl, pp)

PLIST   *pl;
PROC    *pp;

{
    if ( pl->last != NIL_PROC )
    {
        pl->last->next = pp;
        pp->prev       = pl->last;
    }
    else
    {
        pl->first = pp;
        pp->prev  = NIL_PROC;
    }

    pp->next = NIL_PROC;
    pl->last = pp;
}

/*---------------------------------------------------------------*/

static void take_out(pl, pp)

PLIST   *pl;
PROC    *pp;

{
    if ( pl->first == pp )
        pl->first = pp->next;

    if ( pl->last == pp )
        pl->last = pp->prev;

    if ( pp->next != NIL_PROC )
        pp->next->prev = pp->prev;

    if ( pp->prev != NIL_PROC )
        pp->prev->next = pp->next;
}

/*---------------------------------------------------------------*/

static void wake_up(pp)

PROC    *pp;

{
    if ( pp->state != SLEEP )
        return;

    pp->state = READY; 
 
    if ( ++pp->pri >= N_PRI ) 
        pp->pri = (SHORT) (N_PRI - 1); 
 
    append_ready(pp); 
}

/*---------------------------------------------------------------*/ 

static void abort(pid, xstat, dump)

INT     pid;
INT     xstat;
int     dump;

{
    PROC    *pp  = find_proc(pid); 
    PROC    *ppp; 
    INT     ppid, sig;
    int     waited, new_pid, fd; 
    PROC    *xp;
    PLIST   *pl;

    if ( pp == NIL_PROC || pp->state == EXITING || pp->state == ZOMBIE )
        return;

    if ( pp->state == READY )
    {
        pl = &rlist[pp->pri];
        take_out(pl, pp);
    }

    pp->state = EXITING;
    pp->xstat = xstat;

    /* tell parent about death of child */  
 
    ppp    = find_proc(pp->ppid);
    ppid   = ppp->pid;
    waited = dead_child(ppp, pid, xstat); 
 
    /* let the process init adopt all orphans   */

    for ( xp = procs; xp < procs + N_PROCS; ++xp )
        if ( xp->pid != 0 && xp->ppid == pid )
            xp->ppid = INIT_PID;

    if ( dump ) 
    {
        fm_coredump(pid, &fd);

        if ( fd != -1 )
        {
            mm_dump(pid, fd);
            fm_enddump(pid, fd);
        }
    } 
  
    (void) mm_exit(pid);
    (void) fm_exit(pid);
    (void) ipc_exit(pid);

    pp->state = ZOMBIE;

    /* if parent waited or ignoring death of child, don't make child zombie */

    if ( ppp->pid == ppid )     /* could have died in the meantime! */
    {
        if ( waited || ppp->sighdlr[SIGCLD] == SIG_IGN )
        {
            ppp->ctime += pp->utime + pp->ctime;

            release_pid(pid);
            pp->pid = 0;
        }
        else if ( ppp->sighdlr[SIGCLD] != SIG_DFL )
            ppp->sigs |= (1 << SIGCLD); 
    }

    if ( running == NIL_PROC )
        new_pid = -1;
    else if ( running->pid == pid )
        new_pid = schedule();
    else
        new_pid = (int) running->pid;

    sig = ( running == NIL_PROC ) ? 0 : running->sigs;

    ker_exit(pid, new_pid, sig);
}

/*---------------------------------------------------------------*/

static int kill_one(pp, pid, sig)

PROC    *pp;
INT     pid;
INT     sig;

{
    PROC    *kpp = find_proc(pid);

    if ( kpp == NIL_PROC )
        ERROR(ESRCH)

    if ( pp->pid != pid && pp->euid != ROOT_UID &&
            pp->euid != kpp->uid )
        ERROR(EPERM)

    if ( kpp->sighdlr[sig] == SIG_IGN )
        return 0;

    if ( kpp->pause )
        wake_up(kpp);

    kpp->sigs |= (1 << sig); 

    return 0;
}

/*---------------------------------------------------------------*/

static int schedule()

{
    running = next_ready();

    if ( running == NIL_PROC )
        return -1;

    running->state = RUNNING;

    return (int) running->pid;
}

/*---------------------------------------------------------------*/

static void recompute()

{
    PLIST   *pl;
    PROC    *pp, *np;
    SHORT   new_pri;

    for ( pl = rlist; pl < rlist + N_PRI; ++pl )
        for ( pp = pl->first; pp != NIL_PROC; )
        {
            np = pp->next;

            pp->cpu /= 2;
            new_pri  = (SHORT) (pp->cpu / 2) + pp->nice + BASE_PRI;

            if ( new_pri >= N_PRI )
                new_pri = (SHORT) (N_PRI - 1);

            if ( new_pri != pp->pri )
            {
                take_out(pl, pp);

                pp->pri = new_pri;

                append_ready(pp);
            }

            pp = np;
        }

    tick_count = 0;
}

/*---------------------------------------------------------------*/

static int dead_child(pp, cpid, stat)

PROC    *pp;
INT     cpid;
INT     stat;

{
    PROC    *sp;

    for ( sp = wlist.first; sp != NIL_PROC; sp = sp->next )
        if ( sp == pp )
            break;

    if ( sp == NIL_PROC )
        return FALSE;

    if ( --sp->active && pp->sighdlr[SIGCLD] == SIG_IGN )
        return TRUE;

    sp->active = 0;
    take_out(&wlist, sp);

    if ( pp->sighdlr[SIGCLD] == SIG_IGN )
        notify(sp->pid, ECHILD, -1, 0);
    else
        notify(sp->pid, 0, (int)cpid, stat);

    return TRUE;
} 
 
/*---------------------------------------------------------------*/

static INT  issig(pp)

PROC    *pp;

{
    INT     i, mask;
    INT     s = pp->sigs;

    if ( s == 0 )
        return 0;

    for ( i = 0, mask = 1; i < NSIG; ++i, s >>= 1, mask <<= 1 )
        if ( s & 1 )
            break;

    pp->sigs &= ~mask;

    return i;
} 

/*---------------------------------------------------------------*/

static int  core_dump(sig)

INT     sig;

{
    return ( sig > 2 && sig < 13 && sig != 9 );
}

/*---------------------------------------------------------------*/

static INT  new_pid()

{
    INT pid, i, j, n, mask;

    for ( i = next_pid / (8 * sizeof(INT)); i < N_PID; ++i )
        if ( pids[i] != ~0 )
            break;

    while ( i < N_PID )
    {
        for ( j = next_pid % (8 * sizeof(INT)), n = (pids[i] >> j);
                                    j < 8 * sizeof(INT); ++j, n >>= 1 )
            if ( (n & 1) == 0 )
                break;

            if ( j < 8 * sizeof(INT) )
                break;
            else
                ++i;
    }

    mask = ((INT)1 << j);

    pids[i] |= mask;

    next_pid = j + 8 * sizeof(INT) * i;
    pid      = BASE_PID + next_pid++;

    if ( next_pid >= MAX_PID )
        next_pid = 0;

    return pid;
}

/*---------------------------------------------------------------*/

static void release_pid(pid)

INT     pid;

{
    INT     i    = (pid - BASE_PID) / (8 * sizeof(INT));
    INT     j    = (pid - BASE_PID) % (8 * sizeof(INT));
    INT     mask = ((INT)1 << j);

    pids[i] &= ~mask;
}

/*============== Test functions =================================*/

/*lint +fva */
extern int printf();
/*lint -fva */

static int stat_cnt = 0;

void proc_stat()

{
    PROC    *pp;
    CHAR    state;

    if ( running == NIL_PROC )
        return;

    if ( stat_cnt == 0 )
        (void) printf("\nS UID PID PPID PGID CPU PRI NI  TIME\n");
    else
        (void) printf("------------------------------------\n");

    if ( ++stat_cnt == 20 )
        stat_cnt = 0;

    for ( pp = procs; pp < procs + N_PROCS; ++pp )
    {
        if ( pp->pid == 0 )
            continue;

        ++stat_cnt;

        switch ( pp->state )
        {
        case INIT:
            state = 'I';
            break;
        case READY:  
            state = 'R';  
            break;    
        case RUNNING:
            state = 'O';
            break;
        case SLEEP:
            state = 'S';
            break;
        case EXITING:
            state = 'E';
            break;
        case ZOMBIE:
            state = 'Z';
            break;
        }

        if ( state == 'S' )
            (void) printf("%c %3d %3d  %3d  %3d %3d %3d %2d %4d\n",
                                state, pp->uid, pp->pid, pp->ppid, pp->pgid, 
                                pp->cpu, pp->pri, pp->nice,
                                (INT)(pp->utime / HZ));
        else 
            (void) printf("%c %3d %3d  %3d  %3d %3d %3d %2d %4d\n",
                                state, pp->uid, pp->pid, pp->ppid, pp->pgid,
                                pp->cpu, pp->pri, pp->nice,
                                (INT)(pp->utime / HZ));
    }

    if ( stat_cnt >= 20 )
        stat_cnt = 0;
}


