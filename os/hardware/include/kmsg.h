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

#include    "../include/stat.h"

#define MSGKEY_K    2001 
#define MSGKEY_U    2002
#define MSGKEY_I    2003
#define MSGKEY_C    2004

#define CRITICAL    1           /* receive only critical messages   */ 
#define UNCRITICAL  2           /* messages that are less important */
#define KER_MSG     3           /* needed for emulation             */

typedef LONG ARGTYPE;

typedef struct
{
    long    mtype;
    INT     class;              /* CRITICAL, UNCRITICAL,...         */
    INT     queue;              /* pid of recipient                 */
    INT     pid;                /* pid of sender                    */
    INT     chan;               /* sleep channel used by managers   */
    INT     cmd;                /* operation desired                */
    int     errno;              /* the error no. after a call       */
    ARGTYPE arg1;               /* the arguments for system calls   */
    ARGTYPE arg2;
    ARGTYPE arg3;
    ARGTYPE arg4;
    ARGTYPE arg5;
    ARGTYPE arg6;
    ARGTYPE arg7;
    ARGTYPE arg8;
    char    str1[64];
    char    str2[64];

} MSG;

typedef struct
{
    long        mtype;
    INT         int_no;
    INT         arg;

} IMSG;

#define NIL_MSG     (MSG *)0

void    msg_init();
MSG     *msg_alloc();
void    msg_free();
MSG     *msg_next();
void    msg_clear();
void    msg_purge();
void    msg_append();
INT     msg_count();

/*--------------------------------------------------------*/

#define DATA_SIZE   1024
#define DKEY_OFF    20

typedef struct
{
    long    mtype;
    char    data[DATA_SIZE];

} DATA_MSG;

DATA_MSG    *msg_d_alloc();
void        msg_d_free();
DATA_MSG    *msg_d_next();
void        msg_d_append();
void        msg_d_clear();
