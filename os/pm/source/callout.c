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
* This module manages the callout table structure. It is used
* for notifying processes that a certain pre-specified time
* has elapsed.
*
***********************************************************
*/

#include    "../include/ktypes.h"

typedef struct entry
{
    INT             ticks;      /* number of ticks after predecssor */
    INT             pid;        /* process wanting to be notified   */
    struct entry    *next;      /* pointer in linked list           */

} ENTRY;

#define NULL_ENTRY  (ENTRY *)0

#define NR_CALLS    128

static ENTRY    calls[NR_CALLS];
static ENTRY    *first;

static ENTRY    *alloc_entry();

/*------------ Definitions of public functions ---------------*/

void call_init()

{
    ENTRY   *ep;

    for ( ep = calls; ep < calls + NR_CALLS; ++ep )
        ep->pid = 0;

    first = NULL_ENTRY;
}

/*------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: call_out                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pid is the process identifier of a process wanting    */
/*    notification at a later time.                         */
/* 2) ticks is the number of clock ticks to elapse until    */
/*    notification.                                         */
/*                                                          */
/* Exit conditions:                                         */
/* 1) If an entry for process pid already exists in the     */
/*    list, it is removed and the time left until notifi-   */
/*    cation is noted.                                      */
/* 2) A new entry is added to the list containing the pid   */
/*    of the process wanting notification. The 'ticks'      */
/*    field of the entry is adjusted to contain the dif-    */
/*    ference to the predecessor (not the absolute number   */
/*    of ticks to elapse).                                  */
/* 3) The return value is the time left until notification  */
/*    on the previous entry resp. 0 if there was none.      */ 
/*                                                          */
/************************************************************/

int call_out(pid, ticks)

INT     pid;
INT     ticks;

{
    ENTRY   *ep, *pep, *nep;
    INT     old_ticks = 0;

    for ( nep = first, pep = NULL_ENTRY; nep != NULL_ENTRY;
                                            pep = nep, nep = nep->next )
    {
        old_ticks += nep->ticks; 

        if ( nep->pid == pid )
            break;
    }

    if ( nep != NULL_ENTRY )
    {
        if ( pep != NULL_ENTRY )
            pep->next = nep->next;
        else
            first = nep->next;
    }
    else
    {
        nep       = alloc_entry();
        old_ticks = 0;
    }

    if ( nep == NULL_ENTRY )        /* forget it; no more room  */
        return -1;

    for ( ep = first, pep = NULL_ENTRY; ep != NULL_ENTRY;
                                            pep = ep, ep = ep->next )
        if ( ep->ticks > ticks )
        {
            ep->ticks -= ticks;
            break;
        }
        else
            ticks -= ep->ticks;

    nep->next  = ep; 
    nep->pid   = pid;
    nep->ticks = ticks;

    if ( pep == NULL_ENTRY )
        first = nep;
    else
        pep->next = nep;

    return (int) old_ticks;
}

/*------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: call_tick                                      */
/*                                                          */
/* Entry conditions:                                        */
/*    ticks is the number of clock ticks since the last     */
/*    call to call_tick.                                    */
/*                                                          */
/* Exit conditions:                                         */
/*    The ticks fields of as many entries as necessary have */
/*    been adjusted. It can happen thereby that several     */
/*    entries get a ticks field of 0.                       */
/*                                                          */
/************************************************************/

void call_tick(ticks)

INT     ticks;

{
    INT     rest; 
    ENTRY   *ep;

    for ( ep = first, rest = ticks; rest && ep != NULL_ENTRY; ep = ep->next )
    {
        if ( rest <= ep->ticks )
        {
            ep->ticks -= rest;
            rest       = 0;
        }
        else
        {
            rest      -= ep->ticks; 
            ep->ticks  = 0;
        }
    }
}

/*------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: call_next                                      */
/*                                                          */
/* Entry conditions:                                        */
/*    None. This function is called to get the entries with */
/*    a zero ticks-value.                                   */
/*                                                          */
/* Exit conditions:                                         */
/* 1) If the 'ticks' field of the first entry in the        */
/*    callout list is 0, the corresponding entry is re-     */
/*    moved from the list and the pid field of this entry   */
/*    is the return value.                                  */
/* 2) In all other cases the return value is 0.             */
/*                                                          */
/************************************************************/

INT call_next()

{
    INT     pid;

    if ( first == NULL_ENTRY || first->ticks ) 
        return 0; 
 
    pid        = first->pid;
    first->pid = 0;
    first      = first->next;

    return pid;
}

/*------------ Definitions of local functions ---------------*/

static ENTRY *alloc_entry()

{
    ENTRY   *ep;

    for ( ep = calls; ep < calls + NR_CALLS; ++ep )
        if ( ep->pid == 0 )
            break;

    return ( ep < calls + NR_CALLS ) ? ep : NULL_ENTRY;
}
