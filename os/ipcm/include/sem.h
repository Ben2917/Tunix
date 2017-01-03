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

#define K_sem

#define SEM_A   0200        /* alter permission        */
#define SEM_R   0400        /* read permission         */

#define SEM_UNDO    010000  /* undo required upon exit */

#define GETNCNT 3   /* get semncnt      */
#define GETPID  4   /* get sempid       */
#define GETVAL  5   /* get semval       */
#define GETALL  6   /* get all semval's */
#define GETZCNT 7   /* get semzcnt      */
#define SETVAL  8   /* set semval       */
#define SETALL  9   /* set all semvals  */


typedef struct
{
    IPC_PERM    perm;       /* operation permission structure */
    int         *pad;       /* a dummy                        */
    SHORT       nsems;      /* no. of semaphores in set       */
    long        otime;      /* time of last semop             */
    long        ctime;      /* time of last change            */

} SEMID;


typedef struct
{
    SHORT   num;    /* semaphore number    */
    short   op;     /* semaphore operation */
    short   flg;    /* operation flags     */

} SEMBUF;

