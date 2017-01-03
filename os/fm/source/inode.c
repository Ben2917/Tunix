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
* This module manages the in-core inodes. Unfortunately it
* seems difficult to treat these as a data abstraction for
* reasons of efficiency. Thus the fields of an inode are (alas)
* directly manipulated in other modules.
*
***********************************************************
*/

#include    "../include/comm.h"
#include    "../include/inode.h"
#include    "../include/buffer.h"
#include    "../include/super.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/pid.h"
#include    "../include/thread.h"

extern int  errno;

#define NR_INODES   128             /* nr. of in-core inodes    */
#define NR_HASH     32              /* that many hash buckets   */
#define HASH_MASK   NR_HASH - 1     /* used by hash hash_fkt    */

#define DIRECT          10          /* number of direct blocks  */ 
#define NO_PER_BLOCK    (BLOCK_SIZE / sizeof(BLKNO))
#define INDIR1          (DIRECT + NO_PER_BLOCK)
#define INDIR2          (INDIR1 + ((long) NO_PER_BLOCK) * NO_PER_BLOCK)

/*------------------- Local Data Declarations --------------*/

static INODE    inodes[NR_INODES];
static INODE    buckets[NR_HASH];
static INODE    free_list;

static BLKNO    bbuf[1024];

/*------------------- Local Function Declarations ----------*/

static INODE    *hash_search();
static SHORT    hash_fkt();
static void     blk_release();

/*------------------- Public Functions ---------------------*/

void ino_init()

{
    int     i;
    INODE   *ip = inodes;
    INODE   *fp = &free_list;
    INODE   *hp = buckets;

    fp->next = fp->prev = fp;

    for ( i = 0; i < NR_HASH; ++i )
        buckets[i].succ = NIL_INODE;

    for ( i = 0; i < NR_INODES; ++i, ++ip )
    {
        ip->refcnt = 0;
        ip->ino    = 0;

        ip->next       = fp->next;
        ip->prev       = fp;
        fp->next->prev = ip; 
        fp->next       = ip;

        ip->succ = hp->succ;
        ip->pred = hp;
        hp->succ = ip;
    }
}

/*----------------------------------------------------------*/

void ino_new(ip)            /* initialize a new inode   */

INODE   *ip;

{
    int     i;

    ip->mode    = 1;        /* just to mark it taken    */
    ip->nlink   = 0;
    ip->size    = 0L;
    ip->mnt_pnt = FALSE;
    ip->refcnt  = 0;

    /* special code for FIFOs   */

    ip->readers = 0;
    ip->writers = 0;
    ip->in      = 0L;
    ip->out     = 0L;

    for ( i = 0; i < DIRECT; ++i )
        ip->direct[i] = (BLKNO)0;

    ip->indir1 = (BLKNO)0; 
    ip->indir2 = (BLKNO)0;
    ip->indir3 = (BLKNO)0;

    ip->flags |= I_DIRTY;           /* write inode to disk  */
}
/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: ino_get                                        */
/*                                                          */
/* Entry conditions:                                        */
/* 1) dev is a legal device number                          */
/* 2) i_no is a legal inode number for this device          */
/*                                                          */
/* Exit conditions:                                         */
/*    The return value is a locked in-core inode            */
/*    agreeing with the corresponding disk inode.           */
/*    This inode is not on the free list but is in          */
/*    the appropriate hash list.                            */
/*                                                          */
/************************************************************/

INODE   *ino_get(dev, i_no)

DEV     dev;
SHORT   i_no;

{
    INODE   *ino, *hp;

    for (;;)
    {
        if ( (ino = hash_search(dev, i_no)) != NIL_INODE )
        {
            if ( ino->flags & I_LOCKED )
            {
                sleep((EVENT)ino);
                continue;
            }

            if ( ino->refcnt == 0 )     /* it's on free list; take it off   */
            {
                ino->next->prev = ino->prev;
                ino->prev->next = ino->next;
            }

            ++ino->refcnt;

            ino->flags |= I_LOCKED;

            return ino;
        }

        /* inode not in inode cache */

        if ( free_list.next == &free_list )     /* out of in-core inodes    */
        {
            errno = ENFILE;
            return NIL_INODE;
        }

        /* remove a new inode from free list */
 
        ino = free_list.next;

        ino->next->prev = ino->prev;
        ino->prev->next = ino->next;

        /* reset inode number and file system */

        ino->dev = dev;
        ino->ino = i_no;

        /* remove inode from old hash queue, place on new one */

        hp = &buckets[hash_fkt(dev, i_no)];

        if ( ino->succ != NIL_INODE )
            ino->succ->pred = ino->pred;
        ino->pred->succ = ino->succ;

        ino->succ = hp->succ;
        ino->pred = hp;
        hp->succ  = ino;

        /* initialize inode */ 
 
        ino->refcnt = 1; 
        ino->flags  = I_LOCKED; 
 
        /* read inode from disk */

        sup_iget(ino);

        return ino;
    }
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: ino_put                                        */
/*                                                          */
/* Entry conditions:                                        */
/* 1) ino points to an in-core inode got with ino_get.      */
/* 2) This inode need not necessarily be locked, but it     */
/*    certainly is not on the free list.                    */
/*                                                          */
/* Exit conditions:                                         */
/* 1) refcnt has been decremented.                          */
/* 2) The inode is no longer locked.                        */
/* 3) If refcnt became 0, the inode is returned to the      */
/*    free list.                                            */
/* 4) If nlink is 0, the disk inode is freed (sup_ifree)    */
/*    and any blocks belonging to the inode are free.       */
/* 5) If the inode was altered ("dirty"), the disk inode    */
/*    is updated and the inode marked "clean".              */
/*                                                          */
/************************************************************/

void ino_put(ino)

INODE *ino;

{
    ino->flags |= I_LOCKED;

    --ino->refcnt;

    if ( ino->refcnt == 0 )
    {
        if ( ino->nlink == 0 )
        {
            ino_trunc(ino);

            ino->mode   = 0;
            ino->flags |= I_DIRTY;

            sup_ifree(ino->dev, ino->ino);
        }

        /* put inode on free list */ 

        ino->prev = free_list.prev;
        ino->next = &free_list;

        free_list.prev->next = ino;
        free_list.prev       = ino;
    }

    /* 
        If file accessed or inode changed or file changed 
        update disk inode. 
    */ 
 
    if ( ino->flags & I_DIRTY )
    {
        ino->ctime = TIME();

        sup_iput(ino);

        ino->flags &= ~I_DIRTY;
    }

    /* release inode lock */

    ino->flags &= ~I_LOCKED;
    wakeup((EVENT)ino); 
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: ino_map                                        */
/*                                                          */
/* Entry conditions:                                        */
/* 1) ino points to a locked in-core inode.                 */
/* 2) offset is a valid file offset for this inode.         */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The block number is determined, in which the byte     */
/*    with file position offset will be found. If the       */
/*    block exists, its number is the return value. If      */
/*    doesn't exist yet and flag == TRUE, a block is        */
/*    allocated (sup_alloc) and its number is the return    */
/*    value. Otherwise (BLKNO)0 is returned.                */
/* 2) *boffsetp is the relative offset within the block.    */
/* 3) *countp is the number of bytes that can be read from  */
/*    this block beginning at offset *boffsetp.             */
/* 4) *rdahdp is either the number of the immediately       */
/*    following block, if there is one, or (BLKNO)0.        */
/*    In situations where it would be complicated to        */
/*    determine *rdahdp (e.g. crossing a boundary between   */
/*    direct and indirect blocks) we don't bother.          */
/*                                                          */
/************************************************************/ 

BLKNO ino_map(ino, offset, boffsetp, countp, rdahdp, flag)

INODE   *ino;
LONG    offset;
INT     *boffsetp;
INT     *countp;
BLKNO   *rdahdp;
int     flag;               /* create a missing block, if flag == TRUE */

{
    BLKNO   log_blk = (BLKNO) (offset / BLOCK_SIZE);
    BLKNO   *phy_blk;
    LONG    div;
    INT     lvl, ino_lvl, index;
    int     blk_dirty = FALSE;
    BUFFER  *bp  = NIL_BLOCK;
    BUFFER  *bpx = NIL_BLOCK;
    BLKNO   *blp;
    
    *boffsetp = (INT) (offset % BLOCK_SIZE);
    *countp   = BLOCK_SIZE - *boffsetp;

    if ( rdahdp != (BLKNO *)0 )
        *rdahdp = (BLKNO)0;                 /* initialize to be safe */

    /* determine level of indirection */

    if ( log_blk >= INDIR2 )
    {
        lvl      = 4;
        phy_blk  = &(ino->indir3);
        log_blk -= INDIR2;
        div      = ((LONG) NO_PER_BLOCK) * ((LONG) NO_PER_BLOCK);
    }
    else if ( log_blk >= INDIR1 )
    {
        lvl      = 3;
        phy_blk  = &(ino->indir2);
        log_blk -= INDIR1;
        div      = (LONG) NO_PER_BLOCK;
    }
    else if ( log_blk >= DIRECT )
    {
        lvl      = 2;
        phy_blk  = &(ino->indir1);
        log_blk -= DIRECT;
        div      = 1L;
    }
    else
    {
        lvl     = 1;
        phy_blk = &ino->direct[log_blk];

        if ( rdahdp != (BLKNO *)0  && log_blk < 9 )
            *rdahdp = ino->direct[log_blk + 1];
    }

    ino_lvl = TRUE;

    while ( lvl-- ) 
    { 
        if ( *phy_blk == (BLKNO)0 )
        {
            if ( flag == TRUE )
            {
                if ( (bpx = sup_alloc(ino->dev)) == NIL_BLOCK )
                {
                    errno = ENOSPC;
                    break;
                }

                *phy_blk = bpx->block;

                if ( ino_lvl )
                    ino->flags |= I_DIRTY;
                else
                    blk_dirty = TRUE;
            }
            else
                break;
        }
        else
            bpx = buf_read(ino->dev, *phy_blk);

        if ( lvl == 0 )
            break;

        blp      = (BLKNO *) dbuf(bpx);
        index    = (INT) (log_blk / div);
        log_blk %= div;
        div     /= (LONG) NO_PER_BLOCK;
        phy_blk  = &blp[index];
  
        if ( rdahdp != (BLKNO *)0 && index < NO_PER_BLOCK - 1 ) 
            *rdahdp = blp[index + 1]; 

        if ( bp != NIL_BLOCK )
        {
            if ( blk_dirty )
                buf_write(bp);
            else        
                buf_release(bp);

            blk_dirty = FALSE;
        }

        bp  = bpx;
        bpx = NIL_BLOCK; 

        ino_lvl = FALSE;
    }

    if ( bp != NIL_BLOCK )
        if ( blk_dirty )
            buf_write(bp);
        else
            buf_release(bp);

    if ( bpx != NIL_BLOCK )
        buf_release(bpx);

    return *phy_blk;
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: ino_trunc                                      */
/*                                                          */
/* ino_trunc has the job of freeing all blocks belonging to */
/* the inode pointed to by ip.                              */
/*                                                          */
/************************************************************/

void ino_trunc(ip)

INODE   *ip;

{
    int     i;
    BLKNO   blkno;
    DEV     dev = ip->dev;
    SHORT   typ = ip->mode & IFMT;

    if ( typ == IFCHR || typ == IFBLK )
        return;

    for ( i = 0; i < DIRECT; ++i )
        if ( (blkno = ip->direct[i]) != (BLKNO)0 )
        {
            ip->direct[i] = (BLKNO)0;
            sup_free(dev, blkno);
        } 

    if ( (blkno = ip->indir1) != (BLKNO)0 )
    {
        ip->indir1 = (BLKNO)0;
        blk_release(dev, blkno, 0);
    }

    if ( (blkno = ip->indir2) != (BLKNO)0 )
    {
        ip->indir2 = (BLKNO)0;
        blk_release(dev, blkno, 1);
    }

    if ( (blkno = ip->indir3) != (BLKNO)0 )
    {
        ip->indir3 = (BLKNO)0;
        blk_release(dev, blkno, 2);
    }

    ip->flags |= I_DIRTY;
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: ino_perms                                      */
/*                                                          */
/* ino_perms returns a 3-bit number built from the per-     */
/* mission bits of the inode pointed to by ino depending    */
/* what the uid and gid of the current process are. The     */
/* bits are:                                                */
/*      1   execute (search) allowed for this process       */
/*      2   write allowed for this process                  */
/*      4   read allowed for this process                   */
/*                                                          */
/************************************************************/

SHORT ino_perms(ino, uid, gid)

INODE   *ino;
SHORT   uid;
SHORT   gid;

{
    SHORT   perms;

    perms = ino->mode & 07;

    if ( ino->gid == gid )
        perms |= ((SHORT)(ino->mode & 070) >> 3);

    if ( ino->uid == uid )
        perms |= ((SHORT)(ino->mode & 0700) >> 6);

    return perms;
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: ino_stat                                       */
/*                                                          */
/* ino_stat fills in the status buffer stbuf with the in-   */
/* formation contained in the inode pointed to by ino.      */
/*                                                          */
/************************************************************/

void ino_stat(ino, stbuf)

INODE       *ino;
struct stat *stbuf;

{
    SHORT   type = ino->mode & IFMT;

    stbuf->tx_dev   = ino->dev;
    stbuf->tx_ino   = ino->ino;
    stbuf->tx_mode  = ino->mode;
    stbuf->tx_nlink = ino->nlink;
    stbuf->tx_uid   = ino->uid;
    stbuf->tx_gid   = ino->gid;
    stbuf->tx_size  = ino->size;
    stbuf->tx_atime = ino->atime;
    stbuf->tx_mtime = ino->mtime;
    stbuf->tx_ctime = ino->ctime;

    if ( type == IFCHR || type == IFBLK )
        stbuf->tx_rdev = (DEV) ino->size;
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: ino_active                                     */
/*                                                          */
/* ino_active examines all inodes in the system to see if   */
/* there is an active one belonging to device dev (other    */
/* than the root inode). If so, it returns TRUE, otherwise  */
/* FALSE.                                                   */
/*                                                          */
/************************************************************/

int ino_active(dev)

DEV dev;

{
    int     i;
    INODE   *ip;

    for ( i = 0; i < NR_HASH; ++i )
        for ( ip = buckets[i].succ; ip != NIL_INODE; ip = ip->succ )
            if ( ip->refcnt && ip->dev == dev && ip->ino != 1 )
                return TRUE;

    return FALSE;
}

/*---------------- Functions used by the Memory Manager --------*/

void ino_blknos(ip, fst_blk, nblks, chan)

INODE   *ip;
BLKNO   fst_blk;
INT     nblks;
INT     chan;

{
    BLKNO   log_blk = fst_blk;
    BLKNO   *phy_blk;
    LONG    div;
    INT     lvl, index, n;
    INT     cnt  = 0;
    BUFFER  *bp  = NIL_BLOCK;
    BUFFER  *bpx = NIL_BLOCK;
    BLKNO   *blp;
    BLKNO   *bnp = bbuf;
    
    while ( cnt < nblks )
    {
        /* determine level of indirection */

        if ( log_blk >= INDIR2 )
        {
            lvl      = 4;
            phy_blk  = &(ip->indir3);
            log_blk -= INDIR2;
            div      = ((LONG) NO_PER_BLOCK) * ((LONG) NO_PER_BLOCK);
        }
        else if ( log_blk >= INDIR1 )
        {
            lvl      = 3;
            phy_blk  = &(ip->indir2);
            log_blk -= INDIR1;
            div      = (LONG) NO_PER_BLOCK;
        }
        else if ( log_blk >= DIRECT )
        {
            lvl      = 2;
            phy_blk  = &(ip->indir1);
            log_blk -= DIRECT;
            div      = 1L;
        }
        else
        {
            lvl     = 1;
            phy_blk = &ip->direct[log_blk];
            n       = DIRECT - (INT) log_blk;
        }

        while ( --lvl ) 
        { 
            if ( *phy_blk == (BLKNO)0 )
                break;
            else
                bpx = buf_read(ip->dev, *phy_blk);

            blp      = (BLKNO *) dbuf(bpx);
            index    = (INT) (log_blk / div);
            log_blk %= div;
            div     /= (LONG) NO_PER_BLOCK;
            phy_blk  = &blp[index];
            n        = NO_PER_BLOCK - index;
  
            if ( bp != NIL_BLOCK )
                buf_release(bp);

            bp  = bpx;
            bpx = NIL_BLOCK; 
        }

        if ( lvl )  /* these blocks don't exist     */
        {
            n = MIN(NO_PER_BLOCK, nblks - cnt);

            (void) MEMSET(bnp, '\0', n * sizeof(BLKNO));
        }
        else    /* phy_blk points to the first block number we need */  
        {
            n = MIN(n, nblks - cnt);

            (void) MEMCPY(bnp, phy_blk, n * sizeof(BLKNO));
        }

        cnt     += n;
        bnp     += n;
        log_blk += n;

        if ( bp != NIL_BLOCK ) 
            buf_release(bp); 
    }

    data_out(MM_PID, chan, (CHAR *)bbuf, nblks * sizeof(BLKNO));
}

/*---------------- Local Functions -----------------------------*/


/************************************************************/
/*                                                          */
/* FUNCTION: hash_search                                    */
/*                                                          */
/* hash_search searches the appropriate hash list for an    */
/* inode belonging to device dev and with number i_no.      */
/* If such an inode is found, a pointer to it is returned;  */
/* otherwise the null pointer is returned.                  */
/*                                                          */
/************************************************************/

static INODE *hash_search(dev, i_no)

DEV     dev;
SHORT   i_no;

{
    INODE   *ip = &buckets[hash_fkt(dev, i_no)];

    for ( ip = ip->succ; ip != NIL_INODE; ip = ip->succ )
        if ( ip->dev == dev && ip->ino == i_no )
            break;

    return ip;
}

/*---------------------------------------------------------*/

/*lint  -e715   */

static SHORT hash_fkt(dev, i_no)

DEV     dev;
SHORT   i_no;

{
    return (i_no & HASH_MASK);
}

/*lint  +e715   */

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: blk_release                                    */
/*                                                          */
/* blk_release frees (recursively) all the blocks associ-   */
/* ated with an indirect block. lvl == 0 means single in-   */
/* direct, lvl == 1 double indirect, lvl == 2 triple in-    */
/* direct.                                                  */
/*                                                          */
/************************************************************/

static void blk_release(dev, blkno, lvl)

DEV     dev;
BLKNO   blkno;
int     lvl;

{
    BUFFER  *bp = buf_read(dev, blkno);
    BLKNO   *bnp;
    int     i;

    for ( bnp = (BLKNO *) dbuf(bp), i = 0; i < NO_PER_BLOCK; ++i, ++bnp )
        if ( *bnp != (BLKNO)0 )
            if ( lvl )
                blk_release(dev, *bnp, lvl - 1);
            else
                sup_free(dev, *bnp);

    buf_release(bp);

    sup_free(dev, blkno);
}

