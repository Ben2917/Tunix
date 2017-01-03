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
* This module manages the super blocks, disk inodes and the
* list of free blocks. It is also responsible for the mount
* table.
*
* It is the only module that knows what a super block or a
* mount table looks like.
*
***********************************************************
*/

#include    "../include/macros.h"
#include    "../include/devcall.h"
#include    "../include/kcall.h"
#include    "../include/buffer.h" 
#include    "../include/inode.h" 
#include    "../include/super.h"
#include    "../include/errno.h"
#include    "../include/ustat.h"
#include    "../include/thread.h"

#define N_MOUNT     16          /* max. no. of fs's mountable       */
#define N_INOS      128         /* no. of inode numbers stored      */
#define N_BLKS      180         /* no. of block numbers stored      */
#define DIRECT      10          /* no. of direct blocks in inode    */

typedef struct
{
    LONG    tot_size;           /* no. of blocks on this device     */
    INT     ninodes;            /* no. of inodes on minor device    */
    SHORT   iblocks;            /* no. of blocks used by inodes     */
    SHORT   fst_data;           /* no. of first data block          */
    LONG    max_size;           /* max. size of file on this device */
    LONG    blk_free;           /* no. of free blocks on device     */
    INT     ino_free;           /* no. of inodes free on device     */
    SHORT   i_indx;             /* index in inode free list         */
    SHORT   b_indx;             /* index in block free list         */
    SHORT   free_ino[N_INOS];   /* list of free inodes on device    */
    BLKNO   free_blk[N_BLKS];   /* list of free blocks on device    */

} DSUPER;                       /* super block on disk              */


typedef struct
{
    LONG    tot_size;           /* no. of blocks on this device     */
    INT     ninodes;            /* no. of inodes on minor device    */
    SHORT   iblocks;            /* no. of blocks used by inodes     */
    SHORT   fst_data;           /* no. of first data block          */
    LONG    max_size;           /* max. size of file on this device */
    LONG    blk_free;           /* no. of free blocks on device     */
    INT     ino_free;           /* no. of inodes free on device     */
    SHORT   i_indx;             /* index in inode free list         */
    SHORT   b_indx;             /* index in block free list         */
    SHORT   free_ino[N_INOS];   /* list of free inodes on device    */
    BLKNO   free_blk[N_BLKS];   /* list of free block on device     */
    DEV     dev;                /* device on which this sys. lives  */ 
    int     i_lock;             /* is inode free list locked?       */
    int     b_lock;             /* is block free list locked?       */
    INODE   *i_sup;             /* inode for root of this system    */
    INODE   *i_mount;           /* inode on which mounted           */
    int     dirty;              /* has super block changed?         */
    int     rwflag;             /* is it read only?                 */

} MSUPER;                       /* super block in memory            */

static MSUPER   mnt_table[N_MOUNT];

extern int  errno;

/*---------------------- Local Function Declarations ---------------*/

#define ERROR(eno)      { errno = (eno); return -1; }

static MSUPER   *get_super();
static void     collect_free_inodes();
static BUFFER   *get_iblk();

/*---------------------- Public Functions --------------------------*/

void sup_init()

{
    MSUPER  *sp;

    for ( sp = mnt_table; sp < mnt_table + N_MOUNT; ++sp )
        sp->i_sup = NIL_INODE;

    (void) sup_mount(NIL_INODE, NIL_INODE, TRUE);
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_mount                              */
/*                                                  */
/* Entry conditions:                                */
/* 1) dev_ino points to the inode describing the    */
/*    block special device to be mounted.           */
/* 2) dir_ino points to the inode of the directory  */
/*    on which the file system is to be mounted.    */
/*    dir_ino == NIL_INODE in the special case that */
/*    the file system root is being mounted.        */
/* 3) rwflag is a boolean variable.                 */
/*                                                  */
/* Exit conditions:                                 */
/* 1) If there are no more mount table entries free */
/*    errno is EEBUSY and -1 is returned.           */
/* 2) Otherwise a mount table entry is initialized  */
/*    and the refcnts of the inodes dev_ino,        */
/*    dir_ino are incremented to keep these inodes  */
/*    from being taken for somebody else.           */
/* 3) Both inodes are released with ino_put.        */
/*                                                  */
/****************************************************/

int sup_mount(dev_ino, dir_ino, rwflag)

INODE *dev_ino;
INODE *dir_ino;
int   rwflag;

{
    MSUPER  *sp;
    DEV     dev = ( dev_ino == NIL_INODE ) ? (DEV)0 : (DEV)dev_ino->size;

    for ( sp = mnt_table; sp < mnt_table + N_MOUNT; ++sp )
        if ( sp->i_sup == NIL_INODE )
            break;

    if ( sp >= mnt_table + N_MOUNT )
        ERROR(EBUSY)

    if ( dev_open(dev) == -1 )
        ERROR(ENXIO)

    buf_copyin(dev, (BLKNO)1, (char *)sp, 0, sizeof(DSUPER), (BLKNO)0);

    sp->dev     = dev;
    sp->i_mount = dir_ino;
    sp->i_lock  = FALSE;
    sp->b_lock  = FALSE;
    sp->dirty   = FALSE;
    sp->rwflag  = rwflag;
    sp->i_sup   = ino_get(dev, ROOT_INO);

    ++sp->i_sup->refcnt;
    ino_put(sp->i_sup);

    if ( dir_ino != NIL_INODE )
    {
        dir_ino->mnt_pnt = TRUE; 
        ++dir_ino->refcnt;
        ino_put(dir_ino);
    }

    return 0;
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION sup_umount                              */
/*                                                  */
/* Entry conditions:                                */
/*    dev_ino points to the locked inode of the     */
/*    block special device to be unmounted.         */
/*                                                  */
/* Exit conditions:                                 */
/* 1) If the device is not mounted, errno == EINVAL */
/*    and the return value is -1.                   */
/* 2) If the file system is busy (i.e. some files   */
/*    are still in use), errno == EBUSY and the     */
/*    return value is -1. Otherwise:                */
/* 3) The refcnts of the file system root and the   */
/*    mount point are decremented and the inodes    */
/*    are released (ino_put).                       */
/* 4) The inode of the block special file is also   */
/*    released.                                     */
/* 5) The mount table entry is freed.               */
/*                                                  */
/****************************************************/

int sup_umount(dev_ino)

INODE   *dev_ino;

{
    MSUPER  *sp;
    DEV     dev = (DEV) dev_ino->size;

    ino_put(dev_ino);

    for ( sp = mnt_table; sp < mnt_table + N_MOUNT; ++sp )
        if ( sp->dev == dev )
            break;

    if ( sp >= mnt_table + N_MOUNT )
        ERROR(EINVAL)

    if ( ino_active(dev) )
        ERROR(EBUSY)

    /*
        remove shared text entries from region table for
            files belonging to file system; ( chap 7xxx )
    */

    sp->i_sup->flags |= I_LOCKED;
    ino_put(sp->i_sup);         /* decrement refcnt of root inode   */

    dev_close(dev);

    buf_invalid(dev);

    sp->i_mount->flags  |= I_LOCKED;
    sp->i_mount->mnt_pnt = FALSE;
    ino_put(sp->i_mount);       /* decrement refcnt         */

    sp->i_sup = NIL_INODE;      /* free mount table entry   */

    return 0;
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_sync                               */
/*                                                  */
/*   sup_sync write all dirty super blocks back to  */
/*   disk synchronously.                            */
/*                                                  */
/****************************************************/

void sup_sync()

{
    MSUPER  *sp;

    for ( sp = mnt_table; sp < mnt_table + N_MOUNT; ++sp )
        if ( sp->i_sup != NIL_INODE && sp->dirty )
        {
            buf_copyout(sp->dev, (BLKNO)1, (char *)sp, 0,
                                            sizeof(DSUPER), TRUE);
            sp->dirty = FALSE;
        }
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_mounted                            */
/*                                                  */
/* Entry conditions:                                */
/*    mnt_pnt is a pointer to an inode on which a   */
/*    file system is mounted.                       */
/*                                                  */
/* Exit conditions:                                 */
/*    The return value is a pointer to the inode    */
/*    of the root of the mounted file system.       */
/*                                                  */
/****************************************************/

INODE   *sup_mounted(mnt_pnt)

INODE   *mnt_pnt;

{
    MSUPER  *sp;

    for ( sp = mnt_table; sp < mnt_table + N_MOUNT; ++sp )
        if ( sp->i_mount == mnt_pnt )
            return sp->i_sup;

    return NIL_INODE;           /* this shouldn't ever happen   */
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_mnt_point                          */
/*                                                  */
/* Entry conditions:                                */
/*    root is a pointer to the root inode of a      */
/*    mounted file system.                          */
/*                                                  */
/* Exit conditions:                                 */
/*    The return value is a pointer to the inode    */
/*    of the directory on which the system is       */
/*    mounted.                                      */
/*                                                  */
/****************************************************/

INODE   *sup_mnt_point(root)

INODE   *root;

{
    MSUPER  *sp;

    for ( sp = mnt_table; sp < mnt_table + N_MOUNT; ++sp )
        if ( sp->i_sup == root )
            return sp->i_mount;

    return root;        /* must be the absolute root    */
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_ialloc                             */
/*                                                  */
/* Entry conditions:                                */
/*    dev is a legal device number.                 */
/*                                                  */
/* Exit conditions:                                 */
/* 1) In the (hopefully rare) case that there are   */
/*    no more disk inodes available, the return     */
/*    value is NIL_INODE.                           */
/* 2) If the super block list of free inodes was    */ 
/*    empty, further free inodes are sought and     */ 
/*    entered in this list until it is full.        */ 
/* 3) A free disk inode is chosen and an in-core    */
/*    inode for it is allocated.                    */
/* 4) The in-core inode is initialized.             */
/* 5) A pointer to this inode (locked) is returned. */
/*                                                  */
/****************************************************/

INODE   *sup_ialloc(dev)

DEV dev;

{
    MSUPER  *sp = get_super(dev);
    SHORT   ino;
    INODE   *ip;

    for (;;)
    {
        if ( sp->i_lock )
        {
            sleep((EVENT)&sp->i_lock);
            continue;                   /* start all over   */
        }

        if ( sp->i_indx == 0 )
        {
            sp->i_lock = TRUE;          /* lock super block */

            collect_free_inodes(sp);

            sp->i_lock = FALSE;

            wakeup((EVENT)&sp->i_lock);

            if ( sp->i_indx == 0 )      /* no free inodes found */
                return NIL_INODE;
        }

        /* there are inodes in super block inode list   */

        ino = sp->free_ino[--sp->i_indx];
        ip  = ino_get(sp->dev, ino);

        if ( INO_MODE(ip) )             /* not free after all !!!   */
        {
            INO_IS_DIRTY(ip);           /* write inode to disk      */
            ino_put(ip);
            continue;                   /* start all over           */
        }

        /* inode is free; initialize it */

        ino_new(ip);

        --sp->ino_free;
        sp->dirty = TRUE;

        return ip;
    }
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_ifree                              */
/*                                                  */
/* Entry conditions:                                */
/* 1) dev is a legal device number.                 */
/* 2) ino is a legal inode number for this device.  */
/*                                                  */
/* Exit conditions:                                 */
/*    If possible the inode number is added to the  */
/*    list of free inodes. If not, it doesn't       */
/*    matter, since a free inode is always recog-   */
/*    nizable by the fact that mode == 0. This      */
/*    field has hopefully been set zero elsewhere.  */
/*                                                  */
/****************************************************/

void sup_ifree(dev, ino)

DEV     dev;
SHORT   ino;

{
    MSUPER  *sp = get_super(dev);

    /* increment file system free inode count */

    ++sp->ino_free;

    if ( sp->i_lock )
        return;                         /* forget it!       */

    if ( sp->i_indx == N_INOS )         /* list is full     */
    {
        if ( ino < sp->free_ino[0] )    /* ino less than remembered inode */
            sp->free_ino[0] = ino;
    }
    else
        sp->free_ino[sp->i_indx++] = ino;
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_iget                               */
/*                                                  */
/* Entry conditions:                                */
/*    ip is a pointer to a valid in-core inode.     */
/*                                                  */
/* Exit conditions:                                 */
/*    The corresponding disk inode has been copied  */
/*    into the in-core inode.                       */
/*                                                  */
/****************************************************/

void sup_iget(ip)

INODE   *ip;

{
    BLKNO   blkno;
    INT     index;
    MSUPER  *sp = get_super(INO_DEV(ip));

    blkno = FIRST_IBLK + (INO_INO(ip) - 1) / INO_PER_BLK;
    index = ((INO_INO(ip) - 1) % INO_PER_BLK) * DINODE_SIZE;

    buf_copyin(INO_DEV(ip), blkno, (char *)ip, index, DINODE_SIZE, (BLKNO)0);

    if ( sp->rwflag )
        INO_RDWR(ip);
    else
        INO_RDONLY(ip);
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_iput                               */
/*                                                  */
/* Entry conditions:                                */
/*    ip is a pointer to a valid in-core inode.     */
/*                                                  */
/* Exit conditions:                                 */
/*    The corresponding disk inode has been up-     */
/*    dated with the contents of the in-core inode. */
/*                                                  */
/****************************************************/

void sup_iput(ip)

INODE   *ip;

{
    BLKNO   blkno;
    INT     index;

    blkno = FIRST_IBLK + (INO_INO(ip) - 1) / INO_PER_BLK;
    index = ((INO_INO(ip) - 1) % INO_PER_BLK) * DINODE_SIZE;

    buf_copyout(INO_DEV(ip), blkno, (char *)ip, index, DINODE_SIZE, TRUE);
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_alloc                              */
/*                                                  */
/* Entry conditions:                                */
/*    dev is a legal device number.                 */
/*                                                  */
/* Exit conditions:                                 */
/*    The block number of a free block is taken     */
/*    from the free block list. A locked buffer     */
/*    belonging to this device and block number     */
/*    and filled with zeros is returned.            */
/*                                                  */
/*    If the free list is now depleted, then a new  */
/*    block of free block numbers is read in and    */
/*    the free list filled up again.                */
/*                                                  */
/****************************************************/

BUFFER  *sup_alloc(dev)

DEV dev;

{
    MSUPER  *sp = get_super(dev);
    BLKNO   blkno;
    BUFFER  *bp;

    if ( sp->blk_free == 0L )
        return NIL_BLOCK;       /* panic("File system is full"); */

    while ( sp->b_lock )
        sleep((EVENT)&sp->b_lock);

    /* remove block from super block free list */

    blkno = sp->free_blk[--sp->b_indx];

    if ( sp->b_indx == 0 )      /* removed last block from free list */
    {
        sp->b_lock = TRUE;      /* lock super block */

        buf_copyin(dev, blkno, (char *)sp->free_blk,
                                    0, N_BLKS * sizeof(BLKNO), (BLKNO)0);

        sp->b_indx = N_BLKS;

        sp->b_lock = FALSE;     /* unlock super block */
        wakeup((EVENT)&sp->b_lock);
    }

    bp = buf_zero(dev, blkno);

    --sp->blk_free;
    sp->dirty = TRUE;

    return bp;
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: sup_free                               */
/*                                                  */
/* Entry conditions:                                */
/* 1) dev is a legal device number.                 */
/* 2) blkno is a legal block number for the device. */
/*                                                  */
/* Exit conditions:                                 */
/* 1) If the list of free blocks is full, its con-  */
/*    tents are written to the block being freed    */
/*    and it is written to disk.                    */
/* 2) The number of the block is added to the free  */
/*    block list.                                   */
/*                                                  */
/****************************************************/

void sup_free(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    MSUPER  *sp = get_super(dev);

    while ( sp->b_lock )
        sleep((EVENT)&sp->b_lock);

    if ( sp->b_indx == N_BLKS )         /* super block free list full   */
    {
        sp->b_lock = TRUE;

        buf_copyout(dev, blkno, (char *) sp->free_blk, 0,
                                        N_BLKS * sizeof(BLKNO), TRUE);

        sp->b_indx = 0;

        sp->b_lock = FALSE;
        wakeup((EVENT)&sp->b_lock);
    }
        
    /* add block to super block free list */

    sp->free_blk[sp->b_indx++] = blkno;
    ++sp->blk_free;
    sp->dirty = TRUE;
}

/*---------------------------------------------------*/

int sup_ustat(dev, ubufp)

DEV             dev;
struct ustat    *ubufp;

{
    MSUPER  *sp;

    for ( sp = mnt_table; sp < mnt_table + N_MOUNT; ++sp )
        if ( sp->i_sup != NIL_INODE && sp->dev == dev )
            break;

    if ( sp >= mnt_table + N_MOUNT )
        ERROR(EINVAL)

    ubufp->f_tfree    = sp->blk_free;
    ubufp->f_tinode   = sp->ino_free;
    ubufp->f_tname[0] = '\0';
    ubufp->f_tpack[0] = '\0';

    return 0;
}

/*---------------------- Local Functions ---------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: get_super                              */
/*                                                  */
/*    get_super returns a pointer to the in-core    */
/*    super block belonging to the mounted device   */
/*    with number dev.                              */
/*                                                  */
/****************************************************/

static MSUPER   *get_super(dev)

DEV dev;

{
    int i;

    for ( i = 0; i < N_MOUNT; ++i )
        if ( mnt_table[i].dev == dev )
            return &mnt_table[i];

    return (MSUPER *)0;
}

/*---------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: collect_free_inodes                    */
/*                                                  */
/*    collect_free_inodes has the job of looking    */
/*    through all the disk inodes for ones that     */
/*    are free (mode == 0) and adding their numbers */
/*    to the free inode list. It does this until    */
/*    the list is full or there are no more free    */
/*    inodes.                                       */
/*                                                  */
/****************************************************/

static void collect_free_inodes(sp)

MSUPER  *sp;

{
    BUFFER  *bp; 
    D_INODE *ip; 
    SHORT   ino, r_ino, cnt, indx; 

    /* 
        Get remembered inode for free inode search;
        search disk for free inodes until super block full,
        or no more free inodes
    */

    ino  = r_ino = sp->free_ino[0];  
    bp   = get_iblk(sp->dev, ino); 
    ip   = (D_INODE *) dbuf(bp) + (ino - 1) % INO_PER_BLK;
    indx = N_INOS;
    cnt  = 0;  

    do
    {
        if ( INO_MODE(ip) == 0 )                /* this inode is free   */
        {
            sp->free_ino[--indx] = ino;
            ++cnt;
        }

        ++ino;
        ++ip;

        if ( (ino % INO_PER_BLK) == 0 )     /* block boundary   */
        {
            if ( ino > sp->ninodes )
                ino = 1;

            buf_release(bp);

            bp = get_iblk(sp->dev, ino);
            ip = (D_INODE *) dbuf(bp);
        }

    } while ( indx > 0 && ino != r_ino );

    buf_release(bp);

    if ( indx )             /* fewer than N_INOS are free   */
        (void) MEMCPY(sp->free_ino, sp->free_ino + indx, cnt);

    sp->i_indx = cnt;
}

/*---------------------------------------------------*/

static BUFFER *get_iblk(dev, ino)

DEV     dev; 
SHORT   ino;

{
    BLKNO   blkno = (BLKNO) (FIRST_IBLK + (ino - 1) / INO_PER_BLK);

    return buf_read(dev, blkno);
}

/*---------------------------------------------------*/


