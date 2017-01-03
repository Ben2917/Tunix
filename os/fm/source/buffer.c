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
* This module manages the buffer cache. We've almost
* succeeded in treating buffers as a data abstraction.
* The only field of this structure that is used outside
* this module is the field 'data' and that is accessed
* by a macro (dbuf) defined in buffer.h.
*
***********************************************************
*/

#include    "../include/comm.h"
#include    "../include/devcall.h"
#include    "../include/kcall.h"
#include    "../include/buffer.h"
#include    "../include/macros.h"
#include    "../include/thread.h"

extern int  errno;

#define NR_BUFS         63
#define NR_HASH         32              /* that many hash buckets   */
#define HASH_MASK       NR_HASH - 1     /* used by hash_fkt         */

/*-------------- Local Data Declarations ---------------*/

static char     buffers[NR_BUFS * BLOCK_SIZE];
static BUFFER   cache[NR_BUFS];
static BUFFER   buckets[NR_HASH];
static BUFFER   free_list;

/*-------------- Local Function Declarations -----------*/

static BUFFER   *hash_search();
static SHORT    hash_fkt();
static BUFFER   *buf_rdahd();

#define DEV_SRD(b)  dev_sread((b)->dev, ((LONG)BLOCK_SIZE * (b)->block),\
                                            (CHAR *)((b)->data), BLOCK_SIZE)
#define DEV_SWR(b)  dev_swrite((b)->dev, ((LONG)BLOCK_SIZE * (b)->block),\
                                            (CHAR *)((b)->data), BLOCK_SIZE)
#define DEV_ARD(b)  dev_aread((b)->dev, ((LONG)BLOCK_SIZE * (b)->block),\
                                            BLOCK_SIZE, (b))
#define DEV_AWR(b)  dev_awrite((b)->dev, ((LONG)BLOCK_SIZE * (b)->block),\
                                            BLOCK_SIZE, (b))

/*-------------- Public Functions ----------------------*/

void buf_init()

{
    int     i;
    BUFFER  *cp  = cache;
    BUFFER  *cpx = &free_list;
    BUFFER  *hp  = buckets;
    char    *bp  = buffers;

    hp->pred = NIL_BLOCK;

    for ( i = 0; i < NR_BUFS; ++i, cpx = cp++, bp += BLOCK_SIZE )
    {
        cpx->next = cp;                 /* put all buffers on free list */
        cp->prev  = cpx;

        hp->succ = cp;                  /* and on hash-list 0 */
        cp->pred = hp;
        hp       = cp;

        cp->flags = 0;
        cp->data  = bp;
    }

    cpx->succ = NIL_BLOCK;              /* end of hash list */
 
    cpx->next      = &free_list;
    free_list.prev = cpx;               /* end of free list */

    for ( i = 1; i < NR_HASH; ++i ) 
        buckets[i].succ = NIL_BLOCK;    /* other lists start out empty */
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_copyin                                 */
/*                                                      */
/* Entry conditions:                                    */
/* 1) dev is a legal device number.                     */
/* 2) blkno is a legal block number for this device.    */
/* 3) buf points to a buffer of size at least nbytes.   */
/* 4) offset is a byte offset in a buffer, and          */
/*    offset + nbytes <= BLOCK_SIZE.                    */
/*                                                      */
/* Exit conditions:                                     */
/*    nbytes have been copied from the corresponding    */
/*    block to buf.                                     */
/*                                                      */
/********************************************************/

void buf_copyin(dev, blkno, buf, offset, nbytes, rdahd)

DEV     dev;
BLKNO   blkno;
char    *buf;
INT     offset;
INT     nbytes;
BLKNO   rdahd;

{
    BUFFER  *bp;

    if ( rdahd != (BLKNO)0 )
        bp = buf_rdahd(dev, blkno, rdahd);
    else
        bp = buf_read(dev, blkno);

    (void) MEMCPY(buf, bp->data + offset, nbytes);

    buf_release(bp);
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_copyout                                */
/*                                                      */
/* Entry conditions:                                    */
/* 1) dev is a legal device number.                     */
/* 2) blkno is a legal block number for this device.    */
/* 3) buf points to a buffer of size at least nbytes.   */
/* 4) offset is a byte offset in a buffer, and          */
/*    offset + nbytes <= BLOCK_SIZE.                    */
/* 5) immediate is a boolean variable.                  */
/*                                                      */
/* Exit conditions:                                     */
/* 1) nbytes have been copied from buf to the corre-    */
/*    sponding block.                                   */
/* 2) If immediate == TRUE, then the block is scheduled */
/*    for writing immediately (buf_write); otherwise    */
/*    it is marked for delayed write and released.      */ 
/*                                                      */
/********************************************************/

void buf_copyout(dev, blkno, buf, offset, nbytes, immediate)

DEV     dev;
BLKNO   blkno;
char    *buf;
INT     offset;
INT     nbytes;
int     immediate;

{
    BUFFER  *bp;

    if ( nbytes == BLOCK_SIZE )
        bp = buf_fetch(dev, blkno);         /* don't bother to read it  */
    else
        bp = buf_read(dev, blkno);

    (void) MEMCPY(bp->data + offset, buf, nbytes);

    if ( immediate )
        buf_write(bp);
    else
    {
        bp->flags |= F_DELAYED_WRITE;
        buf_release(bp);
    }
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_cp_to_msg                              */
/*                                                      */
/* Entry conditions:                                    */
/* 1) dev is a legal device number.                     */
/* 2) blkno is a legal block number for this device.    */
/* 3) chan is the channel number for sending data.      */
/* 4) offset is a byte offset in a buffer, and          */
/*    offset + nbytes <= BLOCK_SIZE.                    */
/*                                                      */
/* Exit conditions:                                     */
/*    nbytes have been copied from the corresponding    */
/*    block to a data message on channel chan.          */
/*                                                      */
/********************************************************/

void buf_cp_to_msg(dev, blkno, pid, chan, offset, nbytes, rdahd)

DEV     dev;
BLKNO   blkno;
INT     pid;
INT     chan;
INT     offset;
INT     nbytes;
BLKNO   rdahd;

{
    BUFFER  *bp;

    if ( rdahd != (BLKNO)0 )
        bp = buf_rdahd(dev, blkno, rdahd);
    else
        bp = buf_read(dev, blkno);

    data_out(pid, chan, (CHAR *)(bp->data + offset), nbytes);

    buf_release(bp);
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_cp_from_msg                            */
/*                                                      */
/* Entry conditions:                                    */
/* 1) dev is a legal device number.                     */
/* 2) blkno is a legal block number for this device.    */
/* 3) chan is the channel number for receiving data.    */
/* 4) offset is a byte offset in a buffer, and          */
/*    offset + nbytes <= BLOCK_SIZE.                    */
/* 5) immediate is a boolean variable.                  */
/*                                                      */
/* Exit conditions:                                     */
/* 1) nbytes have been copied from a data message to    */
/*    the corresponding block.                          */
/* 2) If immediate == TRUE, then the block is scheduled */
/*    for writing immediately (buf_write); otherwise    */
/*    it is marked for delayed write and released.      */ 
/*                                                      */
/********************************************************/

void buf_cp_from_msg(dev, blkno, pid, chan, offset, nbytes, immediate)

DEV     dev;
BLKNO   blkno;
INT     pid;
INT     chan;
INT     offset;
INT     nbytes;
int     immediate;

{
    BUFFER  *bp;

    if ( nbytes == BLOCK_SIZE )
        bp = buf_fetch(dev, blkno);         /* don't bother to read it  */
    else
        bp = buf_read(dev, blkno);

    data_in(pid, chan, (CHAR *)(bp->data + offset), nbytes);

    if ( immediate )
        buf_write(bp);
    else
    {
        bp->flags |= F_DELAYED_WRITE;
        buf_release(bp);
    }
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_fetch                                  */
/*                                                      */
/* Entry conditions:                                    */
/* 1) dev is a legal device number.                     */
/* 2) blkno is a legal block number for this device.    */
/*                                                      */
/* Exit conditions:                                     */
/* 1) The return value is a pointer to a locked buffer  */
/*    assigned to dev and blkno.                        */
/* 2) If such a buffer was already in the appropriate   */
/*    hash list, it is locked and taken.                */
/* 3) If no such buffer was in the hash list, then the  */
/*    first available buffer from the free list is      */
/*    taken and assigned to dev and blkno. It is put on */
/*    the appropriate hash list.                        */
/* 4) In case 3) any buffers at the front of the free   */
/*    list that were marked DELAYED_WRITE are not taken */
/*    but are scheduled for writing.                    */
/* 5) buf_fetch guarantees nothing about the contents   */
/*    of the returned buffer. It may be INVALID.        */
/*                                                      */
/********************************************************/

BUFFER  *buf_fetch(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    BUFFER  *bp, *hp;

    for (;;)                                /* while buffer not found   */
    {
        bp = hash_search(dev, blkno);

        if ( bp != NIL_BLOCK )              /* if block in hash queue   */
        {                       
            if ( bp->flags & F_LOCKED )     /* Bach: scenario 5 */
            {
                sleep((EVENT)bp);           /* wait for buffer to be free */
                continue;                   /* and start all over         */
            }

            bp->flags |= F_LOCKED;          /* Bach: scenario 1 */

            /* remove buffer from free list */

            bp->prev->next = bp->next;
            bp->next->prev = bp->prev;

            return bp;                      /* return this buffer   */
        }

        while ( free_list.next != &free_list )  /* free list not empty  */
        {
            /* remove first buffer from free list */

            bp = free_list.next;

            bp->prev->next = bp->next;
            bp->next->prev = bp->prev;

            bp->flags |= F_LOCKED;

            if ( bp->flags & F_DELAYED_WRITE )
                /* buffer marked for delayed write; Bach: scenario 3 */
            {
                /* asynchronous write to disk */ 

                bp->flags |= (F_IS_OLD | F_WRITE);
                DEV_AWR(bp);
                continue;                   /* back to inner loop */
            }

            /* Bach: scenario 2 -- found a free buffer */

            bp->dev   = dev;
            bp->block = blkno;
            bp->flags = F_LOCKED;       /* irrelevant what it was before */

            /* remove buffer from old hash queue */

            bp->pred->succ = bp->succ;
            if ( bp->succ != NIL_BLOCK )
                bp->succ->pred = bp->pred;
            
            /* put buffer onto new hash queue */

            hp = &buckets[hash_fkt(dev, blkno)];

            bp->succ = hp->succ;  
            bp->pred = hp; 
 
            if ( hp->succ != NIL_BLOCK )
                hp->succ->pred = bp;
            hp->succ = bp;

            return bp;                      /* return this buffer   */
        }

        /* there are no buffers on free list; Bach: scenario 4 */ 

        sleep((EVENT)&free_list);   /* wait for any buffer */ 
    }
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_release                                */
/*                                                      */
/* Entry conditions:                                    */
/*   buf points to a locked buffer.                     */
/*                                                      */
/* Exit conditions:                                     */
/*   The buffer is unlocked and returned to the free    */
/*   list. If it was old (i.e. previously scheduled     */
/*   for delayed write by buf_fetch) or invalid, it is  */
/*   placed at the front of the free list. Otherwise    */
/*   it is placed at the end of the free list.          */
/*                                                      */
/********************************************************/

void buf_release(buf)

BUFFER  *buf;

{
    if ( (buf->flags & F_IS_OLD) || !(buf->flags & F_VALID) )
    {
        /* if block old or invalid, put at front of free list */

        buf->flags &= ~F_IS_OLD;
        buf->prev   = &free_list;
        buf->next   = free_list.next;

        free_list.next->prev = buf;
        free_list.next       = buf;
    }
    else
    {
        /* otherwise end of free list (LRU)     */

        buf->prev = free_list.prev;
        buf->next = &free_list;

        free_list.prev->next = buf;
        free_list.prev       = buf;
    }

    buf->flags &= (~F_LOCKED);      /* unlock buffer            */
    wakeup((EVENT)&free_list);      /* event any buffer free    */ 
    wakeup((EVENT)buf);             /* event this buffer free   */ 
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_read                                   */
/*                                                      */
/*                                                      */
/* Entry conditions:                                    */
/* 1) dev is a legal device number.                     */
/* 2) blkno is a legal block number for this device.    */
/*                                                      */
/* Exit conditions:                                     */
/* 1) The return value is a pointer to a locked buffer  */
/*    assigned to dev and blkno.                        */
/* 2) If such a buffer was already in the appropriate   */
/*    hash list, it is locked and taken.                */
/* 3) If no such buffer was in the hash list, then the  */
/*    first available buffer from the free list is      */
/*    taken and assigned to dev and blkno. It is put on */
/*    the appropriate hash list.                        */
/* 4) In case 3) any buffers at the front of the free   */
/*    list that were marked DELAYED_WRITE are not taken */
/*    but are scheduled for writing.                    */
/* 5) The contents of the buffer are guaranteed to be   */
/*    identical with those of the corresponding block   */
/*    on the disk, i.e. the buffer is VALID.            */
/*                                                      */
/********************************************************/

BUFFER *buf_read(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    BUFFER  *bp;

    for (;;)
    {
        bp = buf_fetch(dev, blkno);

        if ( !(bp->flags & F_VALID) )
        {
            bp->flags &= ~F_WRITE;
            (void) DEV_SRD(bp);
            buf_iodone(bp, errno);
            continue;
        }

        return bp;
    }
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_zero                                   */
/*                                                      */
/* Entry conditions:                                    */
/* 1) dev is a legal device number.                     */
/* 2) blkno is a legal block number for this device.    */
/*                                                      */
/* Exit conditions:                                     */
/*    The return value is a pointer to a locked buffer  */
/*    belonging to device dev and block number blkno,   */
/*    filled with zeros and marked valid.               */
/*                                                      */
/********************************************************/

BUFFER  *buf_zero(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    BUFFER  *bp = buf_fetch(dev, blkno);

    (void) MEMSET(bp->data, '\0', BLOCK_SIZE);

    bp->flags |= F_VALID;

    return bp;
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_write                                  */
/*                                                      */
/* Entry conditions:                                    */
/*   buf is a locked buffer.                            */
/*                                                      */
/* Exit conditions:                                     */
/*   The buffer is scheduled for an immediate write.    */
/*   When the operation is complete, it will be re-     */
/*   turned to the free list (by buf_iodone).           */
/*   This function is chiefly used for critical blocks  */
/*   like super blocks and inode blocks.                */
/*                                                      */
/********************************************************/

void buf_write(buf)

BUFFER  *buf;

{
    buf->flags |= F_WRITE;
    (void) DEV_SWR(buf);
    buf_iodone(buf, errno);
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_sync                                   */
/*                                                      */
/*   buf_sync flushes all buffers marked for delayed    */
/*   write to disk.                                     */
/*                                                      */
/********************************************************/

void buf_sync()

{
    BUFFER  *bp, *bpx;

    for ( bp = free_list.next; bp != &free_list; )
    {
        bpx = bp;
        bp  = bp->next;

        if ( (bpx->flags & F_DELAYED_WRITE) == 0 )
            continue;

        bpx->flags |= (F_IS_OLD | F_LOCKED);

        bpx->next->prev = bpx->prev;
        bpx->prev->next = bpx->next;

        bpx->flags |= F_WRITE;
        (void) DEV_SWR(bpx);
        buf_iodone(bpx, errno); 
    }
}
        
/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_invalid                                */
/*                                                      */
/*   buf_invalid marks all buffers belonging to device  */
/*   dev as being invalid (used by umount).             */
/*                                                      */
/********************************************************/


void buf_invalid(dev)

DEV dev;

{
    BUFFER  *bp;

    for ( bp = free_list.next; bp != &free_list; bp = bp->next )
        if ( bp->dev == dev )
            bp->flags &= ~F_VALID;
}

/*---------------------------------------------------------*/

/********************************************************/
/*                                                      */
/* FUNCTION: buf_iodone                                 */
/*                                                      */
/*   buf_iodone is called after a read or write opera-  */
/*   ation. Its main function is to release the buffer  */
/*   (i.e. put it on the free list).                    */
/*                                                      */
/********************************************************/

void buf_iodone(buf, error)

BUFFER  *buf;
int     error;

{
    buf->error = error;

    if ( error )
    {
        buf->flags |= F_ERROR;
        buf->flags &= ~F_VALID;
        panic("I/O error on hard disk");
    }
    else
    {
        buf->flags &= ~F_ERROR;
        buf->flags |= F_VALID;
    }

    buf->flags &= ~F_DELAYED_WRITE;

    buf_release(buf);
}

/*------------ Local Functions ---------------------------------*/

static BUFFER *hash_search(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    BUFFER  *bp = &buckets[hash_fkt(dev, blkno)];

    for ( ; bp != NIL_BLOCK; bp = bp->succ )
        if ( (bp->flags & F_VALID) && bp->dev == dev && bp->block == blkno )
            break;

    return bp;
}

/*---------------------------------------------------------*/

/*lint  -e715   */

static SHORT hash_fkt(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    return (SHORT) (blkno & HASH_MASK);
}

/*lint  +e715   */

/*---------------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: buf_rdahd                              */
/*                                                  */
/*   buf_rdahd performs the same function as        */
/*   buf_read, except that it schedules the         */
/*   asynchronous reading of a second block (the    */
/*   next in the file).                             */
/*                                                  */
/****************************************************/

static BUFFER   *buf_rdahd(dev, blk1, blk2)

DEV     dev;
BLKNO   blk1, blk2;

{
    BUFFER  *bp1, *bp2;

    bp1 = buf_read(dev, blk1);

/*
    bp2 = buf_fetch(dev, blk2);

    if ( !(bp2->flags & F_VALID) )
    {
        bp2->flags &= ~F_WRITE;
        DEV_ARD(bp2);
    }
    else
        buf_release(bp2);
*/

    return bp1;
}



