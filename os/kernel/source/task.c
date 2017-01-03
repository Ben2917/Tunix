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
* This module manages task switches.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/kmsg.h"
#include    "../include/task.h"
#include    "../include/kutil.h"
#include    "../include/macros.h"
#include    "../include/pid.h"

extern MSG  *user_msg;
extern int  critical[];

static void restore_task();

/*----------------- Definition of public functions -------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: task_user                                      */
/*                                                          */
/* Switch to user task with task number tno. Send it the    */
/* first kernel message in its queue unless preempted =     */
/* TRUE.                                                    */
/*                                                          */
/************************************************************/

void task_user(tno, preempted)

INT     tno;
int     preempted;

{
    MSG *m;

    restore_task(tno);

    if ( !preempted )
    {
        m = msg_next(tno, FALSE);

        if ( m != NIL_MSG )         /* m == NIL_MSG shouldn't happen!!  */
        {
            send(m);
            msg_free(m);
        }
    }
}

/*--------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: task_next_sys                                  */
/*                                                          */
/* If the task tno has a message in its queue, return tno.  */
/* Otherwise look for some system task having a message on  */
/* its queue and return its number. If there is none,       */
/* return 0.                                                */
/*                                                          */ 
/************************************************************/

INT task_next_sys(tno)

INT     tno;

{
    int     i;

    if ( tno != 0 && msg_there(tno) )
        return tno;

    for ( i = USR_TASKS; i < ALL_TASKS; ++i )
        if ( msg_there(i) )
            return i;

    return 0;
}

/*--------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: task_system                                    */
/*                                                          */
/* Switch to the system task tno. Send it the first message */
/* on its queue.                                            */
/*                                                          */ 
/************************************************************/

void task_system(tno)

INT     tno;

{
    MSG     *m;
    INT     crit = ( tno >= USR_TASKS ) ? critical[tno - USR_TASKS] : FALSE;

    restore_task(tno);

    m = msg_next(tno, crit);
    send(m);
    msg_free(m);
} 

/*--------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: task_sig_hdlr                                  */
/*                                                          */
/* Call the signal handler for user task tno and hand it    */
/* message m.                                               */
/*                                                          */ 
/************************************************************/

void task_sig_hdlr(tno, m)

INT     tno;
MSG     *m;

{
    sig_hdlr(m);
}

/*--------------------------------------------------------------*/

/*lint  -e715   */

static void restore_task(tno)

INT     tno;

{}

/*lint +e715    */
