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

#include    "../include/stat.h"

typedef struct                  /* disk inode               */
{
    SHORT           mode;       /* see #defines below       */
    SHORT           nlink;      /* number of links to inode */
    SHORT           uid;        /* user id of owner         */
    SHORT           gid;        /* group id of owner        */
    LONG            size;       /* file size in bytes       */
    long            atime;      /* time of last access      */
    long            mtime;      /* time of last change      */
    long            ctime;      /* time of last inode mod.  */
    BLKNO           direct[10]; /* the direct blocks        */
    BLKNO           indir1;     /* the single indirect blk. */
    BLKNO           indir2;     /* the double indirect blk. */
    BLKNO           indir3;     /* the triple indirect blk. */

} D_INODE;

typedef struct inode            /* in-core inode            */
{
    SHORT           mode;       /* see #defines below       */
    SHORT           nlink;      /* number of links to inode */
    SHORT           uid;        /* user id of owner         */
    SHORT           gid;        /* group id of owner        */
    LONG            size;       /* file size in bytes       */
    long            atime;      /* time of last access      */
    long            mtime;      /* time of last change      */
    long            ctime;      /* time of last inode mod.  */
    BLKNO           direct[10]; /* the direct blocks        */
    BLKNO           indir1;     /* the single indirect blk. */
    BLKNO           indir2;     /* the double indirect blk. */
    BLKNO           indir3;     /* the triple indirect blk. */
    SHORT           flags;      /* status of in-core inode  */
    DEV             dev;        /* device in which it lives */ 
    SHORT           ino;        /* inode number on device   */
    SHORT           refcnt;     /* how many references      */
    SHORT           mnt_pnt;    /* is this a mount point?   */
    SHORT           rofs;       /* is this a read-only fs?  */
    struct inode    *next;      /* pointers in free list    */
    struct inode    *prev;
    struct inode    *succ;      /* pointers in hash list    */
    struct inode    *pred;

    /* special fields for FIFOs     */

    INT             readers;    /* no. of readers on pipe   */
    INT             writers;    /* no. of writers on pipe   */
    LONG            in;         /* offset for next write    */
    LONG            out;        /* offset for next read     */

} INODE;

#define NIL_INODE   (INODE *)0

/* Masks for flags */

#define I_LOCKED        1           /* in-core inode is locked  */
#define I_DEMAND        2           /* some process wants it    */
#define I_DIRTY         4           /* differs from disk inode  */
#define I_MOUNT         8           /* is a mount point         */


#define FIRST_IBLK          2
#define DINODE_SIZE         sizeof(D_INODE)
#define INO_PER_BLK         (BLOCK_SIZE / DINODE_SIZE)
#define ROOT_INO            1

/*------------- Macros for manipulating inodes ------------------*/

#define INO_MODE(ip)    ((ip)->mode)
#define INO_UID(ip)     ((ip)->uid)
#define INO_GID(ip)     ((ip)->gid)
#define INO_NLINK(ip)   ((ip)->nlink)
#define INO_SIZE(ip)    ((ip)->size)
#define INO_ATIME(ip)   ((ip)->atime)
#define INO_MTIME(ip)   ((ip)->mtime)
#define INO_CTIME(ip)   ((ip)->ctime)
#define INO_FLAGS(ip)   ((ip)->flags)
#define INO_DEV(ip)     ((ip)->dev)
#define INO_INO(ip)     ((ip)->ino)
#define INO_REFCNT(ip)  ((ip)->refcnt)
#define INO_MNTPNT(ip)  ((ip)->mnt_pnt)
#define INO_ROFS(ip)    ((ip)->rofs)
#define INO_RDRS(ip)    ((ip)->readers)
#define INO_WRTS(ip)    ((ip)->writers)
#define INO_IN(ip)      ((ip)->in)
#define INO_OUT(ip)     ((ip)->out)
#define INO_FSTBLK(ip)  ((ip)->direct[0])

#define INO_SET_MODE(ip, m)     ((ip)->mode = (m))
#define INO_SET_OWNER(ip, u, g) (ip)->uid = (u); (ip)->gid = (g)
#define INO_LINK(ip)            ((ip)->nlink++)
#define INO_UNLINK(ip)          ((ip)->nlink--)
#define INO_SET_SIZE(ip, s)     ((ip)->size = (s))
#define INO_SET_ATIME(ip, t)    ((ip)->atime = (t))
#define INO_SET_MTIME(ip, t)    ((ip)->mtime = (t))
#define INO_SET_CTIME(ip, t)    ((ip)->ctime = (t))
#define INO_IS_DIRTY(ip)        ((ip)->flags |= I_DIRTY)
#define INO_SET_DEV(ip, d)      ((ip)->dev = (d))
#define INO_INCREF(ip)          ((ip)->refcnt++)
#define INO_DECREF(ip)          ((ip)->refcnt--)
#define INO_MOUNT(ip)           ((ip)->mnt_pnt = TRUE)
#define INO_UMOUNT(ip)          ((ip)->mnt_pnt = FALSE)
#define INO_RDONLY(ip)          ((ip)->rofs = TRUE)
#define INO_RDWR(ip)            ((ip)->rofs = FALSE)
#define INO_INCRDRS(ip)         ((ip)->readers++)
#define INO_DECRDRS(ip)         ((ip)->readers--)
#define INO_INCWRTS(ip)         ((ip)->writers++)
#define INO_DECWRTS(ip)         ((ip)->writers--)
#define INO_SET_IN(ip, n)       ((ip)->in = (n))
#define INO_SET_OUT(ip, n)      ((ip)->out = (n))

void    ino_init();
void    ino_new();
INODE   *ino_get();
void    ino_put();
BLKNO   ino_map();
void    ino_trunc();
SHORT   ino_perms();
void    ino_stat();
int     ino_active();
void    ino_blknos();
