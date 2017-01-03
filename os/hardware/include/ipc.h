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

#define	K_ipc

#ifndef HAS_KEY_T
#define HAS_KEY_T
typedef int   key_t;
#endif

typedef struct 
{
    SHORT   uid;    /* owner's user id    */
    SHORT   gid;    /* owner's group id   */
    SHORT   cuid;   /* creator's user id  */
    SHORT   cgid;   /* creator's group id */
    SHORT   mode;   /* access modes       */
    SHORT   seq;    /* sequence number    */
    key_t   key;    /* key                */

} IPC_PERM;

/* Mode bits */

#define IPC_ALLOC   0100000     /* entry currently allocated         */
#define IPC_CREAT   0001000     /* create entry if key doesn't exist */
#define IPC_EXCL    0002000     /* access only if key doesn't exist  */
#define IPC_NOWAIT  0004000     /* non-blocking mode                 */

/* Keys */

#define IPC_PRIVATE (key_t)0    /* private key */

/* Control Commands */

#define IPC_RMID    0   /* remove identifier */
#define IPC_SET     1   /* set options       */
#define IPC_STAT    2   /* get options       */
