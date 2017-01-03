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

#ifndef KTYPES
#include    "../include/ktypes.h"
#endif

#define BLOCK_SIZE      1024

typedef struct buf
{
    struct buf  *next;          /* pointers for free list   */
    struct buf  *prev;
    struct buf  *pred;          /* pointers for hash list   */
    struct buf  *succ;
    DEV         dev;
    BLKNO       block;
    int         flags;
    char        *data;
    char        error;          /* returned after I/O       */

} BUFFER;

/* Flags for buffers */

#define F_VALID             1
#define F_LOCKED            2
#define F_DELAYED_WRITE     4
#define F_IS_OLD            8
#define F_WRITE             16
#define F_ERROR             32
#define F_ASYNC             64

#define NIL_BLOCK       (BUFFER *)0

#define dbuf(bp)    (bp)->data

void    buf_init();
void    buf_copyin();
void    buf_copyout();
void    buf_cp_to_msg();
void    buf_cp_from_msg();
BUFFER  *buf_fetch();
void    buf_release();
BUFFER  *buf_read();
BUFFER  *buf_zero();
void    buf_write();
void    buf_sync();
void    buf_invalid();
void    buf_iodone();
