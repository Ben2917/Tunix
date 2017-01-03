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
* This module manages the information about active processes.
* In particular it handles the per process IPC tables.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/proc.h"
#include    "../include/ipcsem.h"
#include    "../include/pid.h"

#define N_PROCS     128         /* max. no of active processes  */
#define N_UNDOS     256         /* max. no of undo structures   */

typedef struct undo_str
{
    int             semid;
    SHORT           semnum;
    short           semadj;
    struct undo_str *next;

} UNDO;

static UNDO     undos[N_UNDOS];

typedef struct
{
    INT     pid;
    SHORT   uid;
    SHORT   gid;
    UNDO    *undo;

} P_DESC;

static P_DESC   procs[N_PROCS];

static UNDO     *new_undo();

/*--------------------------------------------------------*/

void proc_init()

{
    P_DESC  *pdp;
    UNDO    *udp;

    for ( pdp = procs; pdp < procs + N_PROCS; ++pdp )
        pdp->pid = 0;

    pdp = procs;

    pdp->pid = INIT_PID;
    pdp->uid = ROOT_UID;
    pdp->gid = ROOT_GID;

    for ( udp = undos; udp < undos + N_UNDOS; ++udp )
        udp->semid = -1;
}

/*--------------------------------------------------------*/

void proc_fork(child, parent)

INT     child; 
INT     parent;

{
    P_DESC  *pdpp;
    P_DESC  *pdpc = (P_DESC *)0;

    for ( pdpp = procs; pdpp < procs + N_PROCS; ++pdpp )
    {
        if ( pdpp->pid == 0 && pdpc == (P_DESC *)0 )
            pdpc = pdpp;

        if ( pdpp->pid == parent )
            break;
    }

    if ( pdpc == (P_DESC *)0 )
        for ( pdpc = pdpp; pdpc < procs + N_PROCS; ++pdpc )
            if ( pdpc->pid == 0 )
                break;

    pdpc->pid  = child;
    pdpc->uid  = pdpp->uid;
    pdpc->gid  = pdpp->gid;
    pdpc->undo = (UNDO *)0;
}

/*--------------------------------------------------------*/

void proc_exit(pid)

INT     pid;

{
    P_DESC  *pdp;
    UNDO    *udp;

    for ( pdp = procs; pdp < procs + N_PROCS; ++pdp )
        if ( pdp->pid == pid )
            break;

    if ( pdp == (P_DESC *)0 )
        return;

    for ( udp = pdp->undo; udp != (UNDO *)0; udp = udp->next )
    {
        sem_adj(udp->semid, udp->semnum, udp->semadj);
        udp->semid = -1;
    }

    pdp->pid = 0;
}

/*--------------------------------------------------------*/
 
void proc_setuid(pid, uid, gid)

INT     pid;
SHORT   uid;
SHORT   gid;

{
    P_DESC  *pdp;

    for ( pdp = procs; pdp < procs + N_PROCS; ++pdp )
        if ( pdp->pid == pid )
            break;

    if ( pdp == (P_DESC *)0 )
        return;

    pdp->uid = uid;
    pdp->gid = gid;
}

/*--------------------------------------------------------*/

void proc_getuid(pid, uidp, gidp)

INT     pid;
SHORT   *uidp;
SHORT   *gidp;

{
    P_DESC  *pdp;

    for ( pdp = procs; pdp < procs + N_PROCS; ++pdp )
        if ( pdp->pid == pid )
            break;

    if ( pdp == (P_DESC *)0 )
    {
        *uidp = 0;
        *gidp = 0;
    }
    else
    {
        *uidp = pdp->uid;
        *gidp = pdp->gid;
    }
}

/*--------------------------------------------------------*/

int proc_adj(pid, semid, semnum, semop)

INT     pid;
int     semid;
SHORT   semnum;
short   semop;

{
    P_DESC  *pdp;
    UNDO    *udp, *udq, *udn;

    for ( pdp = procs; pdp < procs + N_PROCS; ++pdp )
        if ( pdp->pid == pid )
            break;

    if ( pdp == (P_DESC *)0 )
        return -1;

    for ( udp = pdp->undo, udq = (UNDO *)0; udp != (UNDO *)0;
                                        udq = udp, udp = udp->next )
        if ( udp->semid > semid ||
                        (udp->semid == semid && udp->semnum >= semnum) )
            break;

    if ( udp == (UNDO *)0     ||
         udp->semid  != semid ||
         udp->semnum != semnum )
    { 
        if ( (udn = new_undo()) == (UNDO *)0 )
            return -1;

        udn->next = udp;
        udp       = udn;

        if ( udq == (UNDO *)0 )
            pdp->undo = udp;
        else
            udq->next = udp;

        udp->semid      = semid;
        udp->semnum     = semnum;
    }

    udp->semadj -= semop;

    if ( udp->semadj == 0 )
    {
        if ( udq == (UNDO *)0 )
            pdp->undo->next = udp->next;
        else
            udq->next = udp->next;

        udp->semid      = -1;
    }

    return 0;
}

/*--------------------------------------------------------*/

static UNDO *new_undo()

{
    UNDO    *udp;

    for ( udp = undos; udp < undos + N_UNDOS; ++udp )
        if ( udp->semid == -1 )
            break;

    return udp;
}
