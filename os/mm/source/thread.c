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
* This module makes it possible to have independent threads
* in a process. They can synchronize their work by
* using the functions sleep and wakeup.
*
* When no thread can run, the context saved by init_threads
* is chosen.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/kcall.h"
#include    "../include/thread.h"

extern void panic();

#define N_THREADS   16
#define STK_SIZE    1024
#define NR_REGS     8

typedef struct
{
    int     state;
    EVENT   event;
    int     regs[NR_REGS];
    int     stack[STK_SIZE];

} THREAD;

#define FREE    0
#define ACTIVE  1
#define ASLEEP  2

int     thread_cnt;

static THREAD   threads[N_THREADS];
static int      this_proc;
static int      main_regs[NR_REGS];

extern void new_context();
extern void save_context();
extern void restore_context();
extern void switch_context();
 
void sleep();

static void dummy1();
static void dummy2();

/* ---------------------------------------- */

void init_threads()

{
    int i;

	dummy1(&i);
	save_context(main_regs);

    for (i = 0; i < N_THREADS; ++i)
    {
        threads[i].state    = FREE;
        threads[i].stack[0] = 5555;
    }

    thread_cnt = N_THREADS;
}

/* ---------------------------------------- */

void start_thread()

{
    int     *sp;
    THREAD  *thp;

    if ( thread_cnt <= 0 )
        return;

    for ( thp = threads; thp < threads + N_THREADS; ++thp )
        if ( thp->state == FREE )
            break;

    if ( thp >= threads + N_THREADS )
    {
        panic("threads all used up");
        return;
    }                         

    this_proc  = thp - threads;
    sp         = thp->stack + STK_SIZE - 1;
    thp->state = ACTIVE;
    --thread_cnt;

    new_context(sp);
}

/* ---------------------------------------- */

void end_thread()

{
    int old_proc = this_proc;
    int *next_regs;

    ++thread_cnt;

    threads[this_proc].state = FREE;

    if ( threads[this_proc].stack[0] != 5555 )
        panic("stack overflow in thread");

    for ( ; ; )
    {
        if ( threads[this_proc].state == ACTIVE )
            break;

        this_proc = (this_proc + 1) % N_THREADS;

        if ( this_proc == old_proc )
            break;
    }

    if ( this_proc == old_proc )
        next_regs = main_regs;
    else
        next_regs = threads[this_proc].regs;

    restore_context(next_regs);
}

/* ---------------------------------------- */

void sleep(event)

EVENT   event;

{
    int old_proc = this_proc;
    int *next_regs;

	dummy2(&old_proc, &next_regs);
	save_context (threads[old_proc].regs);

    if ( threads[this_proc].stack[0] != 5555 )
        panic("stack overflow in thread");

    threads[this_proc].state = ASLEEP;
    threads[this_proc].event = event;

    for ( ; ; )
    {
        if ( threads[this_proc].state == ACTIVE )
            break;

        this_proc = (this_proc + 1) % N_THREADS;

        if ( this_proc == old_proc )
            break;
    }

    if ( this_proc == old_proc )
        next_regs = main_regs;
    else
        next_regs = threads[this_proc].regs;

    restore_context(next_regs);
}

/* ---------------------------------------- */

void wakeup(event)

EVENT   event;

{
    THREAD  *thp;

    for ( thp = threads; thp < threads + N_THREADS; ++thp )
        if ( thp->state == ASLEEP && thp->event == event )
            thp->state = ACTIVE;
}

/* ---------------------------------------- */

static void dummy1 (ip)

int*	ip;

{
	/* prevent the compiler from using register variables! */
}
/* ---------------------------------------- */

static void dummy2 (ip, cpp)

int*	ip;
int**	cpp;

{
	/* prevent the compiler from using register variables! */
}


