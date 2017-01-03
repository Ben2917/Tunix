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
* This small module is the heart of the operating system.
* It is responsible for routing the kernel and data messages
* between the various processes and seeing to it that the
* correct task is running.
*
* It runs a system process if there is one that has anything
* to do (i.e. a message waiting on its queue). Otherwise it
* runs the user process designated by the process manager.
* The number of this process is stored in crnt_pid.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/kmsg.h"
#include    "../include/fcodes.h"
#include    "../include/gate.h"
#include    "../include/task.h"
#include    "../include/memory.h"
#include    "../include/fault.h"
#include    "../include/pid.h"
#include    "../include/kutil.h"
#include    "../include/macros.h"
#include    "../include/signal.h"

extern int  pause();

typedef struct
{
    int     pid;
    int     state;
    INT     pd;

} TASK;

/* States a task can have   */

#define RUNNING     1
#define LISTENING   2
#define SLEEPING    3
#define PREEMPTED   4

#define HZ          60          /* number of ticks per time slice       */

static INT  sys_task;
static INT  user_task;
static TASK utasks[ALL_TASKS];
static INT  tick_count;
static INT  slice;

int     crnt_queue;
int     crnt_pid; 
int     critical[SYS_TASKS];  
 
static INT  pid_to_task();
static void ker_request();
static void sys_send();
static void sys_receive();
static void sys_request();
static void switch_user_task();
static void init_msg();
static MSG  *new_msg();

static void page_fault();
static void clock_interrupt();
static void mem_interrupt();
static void dev_interrupt();

#define NONE    (HDLR)0

static HDLR handlers[20] =
{
    NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,
    NONE, NONE, NONE, NONE, NONE, page_fault, page_fault,
    clock_interrupt, mem_interrupt, mem_interrupt,
    dev_interrupt, dev_interrupt
};

/*----------------- Definition of public functions ------------------*/

void gate_init()

{
    INT     i;
    TASK    *pt;
    MSG     *pm;

    msg_init();

    for ( pt = utasks; pt < utasks + ALL_TASKS; ++pt )
        pt->pid   = 0;

    utasks[0].pid         = -1;
    utasks[USER_TASK].pid = INIT_PID; 
    utasks[FM_TASK].pid   = FM_PID;
    utasks[MM_TASK].pid   = MM_PID;
    utasks[IM_TASK].pid   = IM_PID;
    utasks[PM_TASK].pid   = PM_PID; 
    utasks[HD_TASK].pid   = MAX_PID;        /* devhd    */
    utasks[SW_TASK].pid   = MAX_PID + 1;    /* devsw    */
    utasks[TY_TASK].pid   = MAX_PID + 2;    /* devtty   */
  
    for ( i = USR_TASKS; i < ALL_TASKS; ++i ) 
    { 
        utasks[i].state = LISTENING;
        init_msg(i); 
        critical[i - USR_TASKS] = FALSE; 
    } 
 
    utasks[USER_TASK].state = RUNNING;

    crnt_pid  = INIT_PID;
    user_task = USER_TASK;

    sys_task   = FM_TASK;
    crnt_queue = FM_PID;

    tick_count = 0;

    pm = new_msg();

    pm->class = UNCRITICAL;
    pm->cmd   = EXEC;
    pm->queue = PM_PID;
    pm->pid   = 0;

    (void) STRCPY(pm->str1, "init");

    msg_append(PM_TASK, pm);

    task_system(sys_task);
}   

/*------------------ Definition of local functions ---------------------*/ 

static INT pid_to_task(pid)

INT     pid;

{
    INT tno;

    tno = ( (pid > 0 && pid < INIT_PID) || pid >= MAX_PID ) ? USR_TASKS : 0;

    for ( ; tno < ALL_TASKS; ++tno )
        if ( utasks[tno].pid == pid )
            break;

    return ( tno < ALL_TASKS ) ? tno : 0;
}

/*----------------------------------------------------------------------*/

void gate_request(m)

MSG     *m;

{
    INT     cmd = m->cmd; 
    INT     tno;

    switch ( cmd )
    {
    case EXIT:
        if ( (tno = pid_to_task((INT) m->arg3)) == 0 )
            break;          /* spurious exit report */

        utasks[tno].pid = 0;

        msg_clear(tno);
        msg_d_clear(tno);
        msg_purge(FM_TASK, (INT) m->arg3);
        msg_purge(PM_TASK, (INT) m->arg3);
        msg_purge(IM_TASK, (INT) m->arg3);
        msg_d_purge(FM_TASK, (INT) m->arg3);
        msg_d_purge(PM_TASK, (INT) m->arg3);
        msg_d_purge(IM_TASK, (INT) m->arg3);

        if ( user_task == tno )
            switch_user_task((int) m->arg1, (int) m->arg2);

        break;
    case CALL_HDLR:
        tno = pid_to_task((INT) m->arg1);
        task_sig_hdlr(tno, m);
        break;
    case PANIC:
        panic(m->str1);
        break;
    case PTABLE:
        (void) ptable((INT) m->arg1);
        break;
    case PTABLE_DUP:
        ptable_dup((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_BUILD:
        pte_build((INT) m->arg1, (INT) m->arg3, (SHORT) m->arg4);
        break;
    case PTE_SET_PRSNT:
        pte_set_prsnt((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_SET_NOTPRSNT:
        pte_set_notprsnt((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_WRITE:
        pte_write((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_NOTWRITE:
        pte_notwrite((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_IS_PRSNT:
        pte_is_prsnt((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_ACCESS:
        pte_access((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_DIRTY:
        pte_dirty((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_RESET:
        pte_reset((INT) m->arg1, (INT) m->arg2);
        break;
    case PTE_SET_FRAME:
        pte_set_frame((INT) m->arg1, (INT) m->arg2, (INT) m->arg3);
        break;
    case PTE_GET_FRAME:
        pte_get_frame((INT) m->arg1, (INT) m->arg2);
        break;
    case MEM_COPY:
        mem_copy((INT) m->arg1, (INT) m->arg2);
        break;
    case KMEM_CPIN:
        kmem_cpin((INT) m->arg1, m->chan);
        break;
    case KMEM_CPOUT:
        kmem_cpout((INT) m->arg1, m->chan);
        break;
    case MEM_ZERO:
        mem_zero((INT) m->arg1);
        break;
    case PD_ALLOC:
        if ( (int) m->arg1 == INIT_PID )
            tno = pid_to_task(INIT_PID);
        else
        {
            tno               = pid_to_task(0);
            utasks[tno].pid   = (int) m->arg1; 
            utasks[tno].state = LISTENING;
        }
        utasks[tno].pd = pd_alloc(tno);
        break;                 
    case PDE_BUILD:
        pde_build((INT) m->arg1, (ADDR) m->arg2, (INT) m->arg3);
        break;
    case ADR2IND_OFF: 
        adr2ind_off((ADDR) m->arg1); 
        break; 

    /* the following cases are answers to system requests   */

    case KILL:
        break;
    case SLEEP:
        switch_user_task((int) m->arg1, (int) m->arg2);
        break;
    case WAKEUP:
        switch_user_task((int) m->arg1, (int) m->arg2);
        break;
    case TICK:
        switch_user_task((int) m->arg1, (int) m->arg2);
        break;
    case PFAULT:
        if ( (int) m->arg1 == -1 )
            sys_request(PM_TASK, KILL, (ARGTYPE) utasks[user_task].pid,
                                                      (ARGTYPE) SIGSEGV);
        break;
    case VFAULT:
        if ( (int) m->arg1 == -1 )
            sys_request(PM_TASK, KILL, (ARGTYPE) utasks[user_task].pid,
                                                      (ARGTYPE) SIGSEGV);
        break;
    default:
        panic("bad command to kernel");
        break; 
    }
}

/*----------------------------------------------------------------------*/

void gate_send(m)

MSG     *m;

{
    MSG     *sm;
    INT     pid = m->queue;
    INT     tno = pid_to_task(pid);

    sm = new_msg();

    (void) MEMCPY(sm, m, sizeof(MSG)); 

    msg_append(tno, sm);

    if ( tno < USR_TASKS && utasks[tno].state == SLEEPING )
    {
        utasks[tno].state = LISTENING;
        sys_request(PM_TASK, (SHORT) WAKEUP, (ARGTYPE) pid,
                                                    (ARGTYPE) tick_count);
        tick_count = 0;
    }
}

/*----------------------------------------------------------------------*/

void gate_receive(m)

MSG     *m;

{
    int preemp;

    if ( sys_task )
    {
        critical[sys_task - USR_TASKS] = (int) m->arg3;

        if ( (sys_task = task_next_sys(sys_task)) != 0 )
        {
            crnt_queue = (INT) utasks[sys_task].pid;
            task_system(sys_task);
        }
        else
        {   
            preemp = (utasks[user_task].state == PREEMPTED);
            slice  = 0;

            utasks[user_task].state = RUNNING;
            crnt_queue              = (INT) utasks[user_task].pid;
            task_user(user_task, preemp);
        }
    }
    else
    {
        utasks[user_task].state = SLEEPING; 
        sys_request(PM_TASK, (SHORT) SLEEP, (ARGTYPE) tick_count, (ARGTYPE) 0);  
        tick_count = 0;

        sys_task   = PM_TASK;  
        crnt_queue = (INT) utasks[sys_task].pid; 
        task_system(sys_task); 
    }   
}

/*----------------------------------------------------------------------*/ 

void gate_data_in(m)

MSG     *m;

{
    DATA_MSG    *dm  = msg_d_alloc();

    if ( dm == (DATA_MSG *)0 )
        panic("data messages all in use");

    rcv_data(m->pid, dm);

    msg_d_append(pid_to_task((INT) m->arg1), m->pid, m->chan, dm);
}

/*----------------------------------------------------------------------*/ 

void gate_data_out(m)


MSG     *m;

{
    INT         task   = pid_to_task(m->pid);
    DATA_MSG    *dm    = msg_d_next(task, (INT) m->arg1, m->chan);

    if ( dm == (DATA_MSG *)0 )
        panic("missing data message");

    snd_data(m->pid, dm);
    msg_d_free(dm);
}

/*-------------------------------------------------------------------*/ 

void gate_handle_interrupt(int_no, arg)

INT     int_no;
INT     arg;

{
    HDLR    hdlr = handlers[int_no];

    (*hdlr)(int_no, arg);

    if ( sys_task == 0 && (sys_task = task_next_sys(0)) != 0 )
    { 
        if ( user_task > 0 && utasks[user_task].state == RUNNING )
            utasks[user_task].state = PREEMPTED;

        crnt_queue = (INT) utasks[sys_task].pid;
        task_system(sys_task);
    }
}

/*----------------------------------------------------------------------*/

static void sys_request(task, cmd, arg1, arg2)

INT     task;
INT     cmd;
ARGTYPE arg1;
ARGTYPE arg2;

{
    MSG     *m  = new_msg();
        
    m->class = UNCRITICAL;
    m->queue = (INT) utasks[task].pid;
    m->pid   = 0;
    m->cmd   = cmd;
    m->arg1  = arg1;
    m->arg2  = arg2;

    mask_interrupts();          /* avoid race conditions here!! */

    msg_prepend(task, m);

    unmask_interrupts();
}

/*----------------------------------------------------------------------*/

static void switch_user_task(pid, sig)

int     pid;
int     sig;

{
    if ( utasks[user_task].state == RUNNING )
        utasks[user_task].state = PREEMPTED;

    crnt_pid = pid;
 
    if ( sig )
        sys_request(PM_TASK, (SHORT) ACT_ON_SIG, (ARGTYPE) pid, (ARGTYPE) 0);

    user_task = pid_to_task((INT) pid);
}

/*----------------------------------------------------------------------*/

static void page_fault(int_no, addr)

INT     int_no;
INT     addr;

{
    INT     indx = adr2ind((ADDR) addr);
    INT     cmd  = ( int_no == VALIDITY ) ? VFAULT : PFAULT;

    sys_request(MM_TASK, cmd, (ARGTYPE) addr, (ARGTYPE) indx);

    if ( sys_task == 0 )        /* this must actually hold!!    */
    {
        sys_request(PM_TASK, (SHORT) TICK, (ARGTYPE) tick_count, (ARGTYPE)0);
        tick_count = 0;
    }
}

/*----------------------------------------------------------------------*/

/*lint  -e715 */

static void clock_interrupt(int_no, ticks)

INT     int_no;
INT     ticks;

{
    tick_count += ticks;

    if ( sys_task == 0 )
    {
        slice += ticks;

        if ( slice >= HZ )
        { 
            sys_request(PM_TASK, (SHORT) TICK, (ARGTYPE) tick_count,
                                               (ARGTYPE) 0);
            tick_count = 0;
        }
    }
}
 
/*----------------------------------------------------------------------*/ 

#define PEEK_INTER  16
#define POKE_INTER  17

static void mem_interrupt(int_no, addr)

INT     int_no;
INT     addr;

{
    /* not implemented at the moment    */
}

/*lint +e715    */ 
 
/*----------------------------------------------------------------------*/ 

#define HD_INTER    18
#define SW_INTER    19

static void dev_interrupt(int_no, chan)

INT     int_no;
INT     chan;

{
    switch ( int_no )
    {
    case HD_INTER:
        sys_request(HD_TASK, (SHORT) INTR, (ARGTYPE) chan, (ARGTYPE)0);
        break;
    case SW_INTER:
        sys_request(SW_TASK, (SHORT) INTR, (ARGTYPE) chan, (ARGTYPE)0);
        break;
    }

    if ( sys_task == 0 )
    {
        sys_request(PM_TASK, (SHORT) TICK, (ARGTYPE) tick_count, (ARGTYPE)0);
        tick_count = 0;
    }
}

/*----------------------------------------------------------------------*/ 

static void init_msg(tno)

INT     tno;

{
    MSG     *mm = new_msg();

    mm->class = CRITICAL;
    mm->queue = (INT) utasks[tno].pid;
    mm->cmd   = INIT;
    mm->pid   = 0;

    msg_append(tno, mm);
}

/*----------------------------------------------------------------------*/ 

static MSG  *new_msg()

{
    MSG *mm = msg_alloc();

    if ( mm == NIL_MSG ) 
        panic("kernel messages all in use");

    return mm;
}

/*----------- Needed for tests ---------------------------------------*/ 

/*lint  -e515   */

extern int  printf();

static char *bez[5] = 
{
    "  NONE   ", " RUNNING ", "LISTENING", "SLEEPING ", "PREEMPTED"
};

void gate_display_state()

{
    INT     i;
    TASK    *t;

    (void) printf("TASK  PID    STATE    MSGS\n");
    for ( i = 1, t = &utasks[i]; i < USR_TASKS; ++i, ++t )
        if ( t->pid )
            (void) printf("%4d  %3d  %9s  %4d\n", i, t->pid,
                                    bez[t->state], msg_count(i));

    (void) printf("\n");
}

/*lint +e515    */
