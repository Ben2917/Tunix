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
*/

#ifndef K_ipc
#include "../include/ipc.h"
#endif

#define MSG_R   0400    /* read permission  */
#define MSG_W   0200    /* write permission */

#define MSG_RWAIT   01000   /* a reader is waiting to receive */
#define MSG_WWAIT   02000   /* a writer is waiting to send    */

#define MSG_NOERROR 010000  /* no error if message too big    */

typedef struct
{
    long    mtype;      /* message type */
    char    mtext[1];   /* message text */

} MSGBUF;

typedef struct
{
    IPC_PERM    perm;   /* operation permission structure */
    SHORT       qnum;   /* no. of messages on queue       */
    SHORT       qbytes; /* max no. of bytes on queue      */
    SHORT       lspid;  /* who executed last msgsnd?      */
    SHORT       lrpid;  /* who executed last msgrcv?      */
    long        stime;  /* time of last msgsnd            */
    long        rtime;  /* time of last msgrcv            */
    long        ctime;  /* time of last change            */

} MSGID;

int     msgget();
int     msgrcv();
int     msgsnd();
int     msgctl();
