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

#define SHM_RDONLY  010000      /* attach in read-only mode         */
#define SHM_RND     020000

typedef struct {
    IPC_PERM    perm;       /* operation permission structure   */
    int         segsz;      /* segment size                     */
    SHORT       lpid;       /* who did last shared mem op.?     */
    SHORT       cpid;       /* pid of creator                   */
    SHORT       nattch;     /* current number attached          */
    long        atime;      /* time of last attach              */
    long        dtime;      /* time of last detach              */
    long        ctime;      /* time of last change              */

} SHMID;      


