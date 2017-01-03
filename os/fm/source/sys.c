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
* This module manages the file table. It is an intermediary
* between fproc (which knows about processes) and the
* primitives of the file system like inodes, buffers etc.
* It gets all the information it needs about a process by
* way of the data structure PDATA.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/comm.h"
#include    "../include/chan.h"
#include    "../include/raw.h"
#include    "../include/devcall.h"
#include    "../include/buffer.h"
#include    "../include/pdata.h" 
#include    "../include/inode.h"
#include    "../include/super.h"
#include    "../include/dir.h"
#include    "../include/sys.h"
#include    "../include/errno.h"
#include    "../include/fcntl.h"
#include    "../include/ahdr.h"
#include    "../include/macros.h"
#include    "../include/sysdep.h"
#include    "../include/ustat.h"
#include    "../include/pid.h"
#include    "../include/thread.h"

extern int  errno;

typedef struct
{
    INODE   *inode;
    LONG    offset;
    INT     refcnt;
    SHORT   mode;

} ENTRY;

#define TABSIZE 256
#define MAXLINK 256
#define MAX_DIR 10 * BLOCK_SIZE /* max. size for a directory        */
#define MAXPIPE 10 * BLOCK_SIZE /* max. capacity of a pipe          */

static ENTRY    ftable[TABSIZE];
 
#define WRBITS   3              /* RD/WR-part of mode argument      */ 
#define TRDMODE  2 
#define TWRMODE  4 
 
/*------------------------------------------------------------------*/

typedef int             BOOLEAN;

/*------------------------------------------------------------------*/

#define ERROR(eno)      { errno = (eno); return -1; }
#define LERROR(eno)     { errno = (eno); return -1L; }

static int      new_entry();
static int      pipe_read();
static int      pipe_write();
static int      raw_read();
static int      raw_write();

/*------------------------------------------------------------------*/

void sys_init()

{
    int i;

    chan_init();
    dev_init();
    buf_init();  
    ino_init();  
    sup_init(); 
 
    for  ( i = 0; i < TABSIZE; ++i )
        ftable[i].inode = NIL_INODE;
}
/*------------------------------------------------------------------*/

int sys_open(pdp, path, mode, perms)

PDATA   *pdp;
char    *path;
SHORT   mode;
SHORT   perms;

{
    SHORT   perm;
    SHORT   type;
    INODE   *ip  = dir_path(pdp, path, FIND);
    LONG    pos, now;

    if ( (mode & O_CREAT) && (mode & O_EXCL) && errno == 0 )
    {
        ino_put(ip); 
        ERROR(EEXIST)
    }

    if ( errno != 0 && errno != ENOENT )
        return -1;

    if ( (mode & O_CREAT) != O_CREAT && errno )
        return -1;

    if ( errno )                    /* try to create new file       */
    {
        if ( (perms & IFMT) && (perms & IFMT) != IFREG )
            ERROR(EINVAL)           /* use mknod for directories etc.   */

        errno = 0;

        ip = dir_path(pdp, path, CREATE);

        if ( errno )
            return -1;              /* create didn't work       */

        INO_SET_MODE(ip, perms | IFREG);
        now = TIME();
        INO_SET_ATIME(ip, now);
        INO_SET_MTIME(ip, now);
        INO_SET_CTIME(ip, now);
        INO_IS_DIRTY(ip);
    }

    if ( INO_ROFS(ip) && (mode & WRBITS) )
    {
        ino_put(ip);
        ERROR(EROFS)
    }

    perm = ino_perms(ip, pdp->uid, pdp->gid);

    if ( ((perm & 02) == 0 && (mode & WRBITS)) ||
         ((perm & 04) == 0 && (mode & WRBITS) != O_WRONLY) )
    {
        ino_put(ip);
        ERROR(EACCES)
    }

    type = INO_MODE(ip) & IFMT;

    if ( type == IFDIR  && (mode & WRBITS) )
    {
        ino_put(ip);
        ERROR(EISDIR)
    }

    if ( type == IFREG && (mode & O_TRUNC) == O_TRUNC && !INO_ROFS(ip) ) 
        ino_trunc(ip); 
 
    pos = ( (mode & O_APPEND) == O_APPEND ) ? INO_SIZE(ip) : 0L;

    INO_INCREF(ip); 

    INO_SET_ATIME(ip, TIME());
    INO_IS_DIRTY(ip);

    ino_put(ip);

    if ( type == IFIFO )
    {
        if ( (mode & WRBITS) )
            INO_INCWRTS(ip);

        if ( (mode & WRBITS) == O_RDONLY || (mode & WRBITS) == O_RDWR )
            INO_INCRDRS(ip);

        if ( (mode & WRBITS) == O_WRONLY )
            while ( INO_RDRS(ip) == 0 )
                sleep((EVENT)&INO_RDRS(ip));

        if ( (mode & WRBITS) == O_RDONLY )
            while ( INO_WRTS(ip) == 0 )
                sleep((EVENT)&INO_WRTS(ip));
    }

    return new_entry(ip, pos, mode);
}
/*------------------------------------------------------------------*/

void sys_close(eno)

int eno;

{
    ENTRY   *ep  = &ftable[eno];
    INODE   *ip  = ep->inode;
    SHORT   mode = (ep->mode) & 3;
    SHORT   type = INO_MODE(ip) & IFMT;

    if ( (--ep->refcnt) == 0 )
    {
        if ( type == IFIFO )
        {
            if ( mode )                                 /*  a writer    */
                INO_DECWRTS(ip);

            if ( mode == O_RDONLY || mode == O_RDWR )   /*  a reader    */
                INO_DECRDRS(ip);

            wakeup((EVENT)&INO_SIZE(ip));   /* in case somebody's waiting   */
        }

        ino_put(ip);
        ep->inode = NIL_INODE;
    }
}

/*------------------------------------------------------------------*/

int sys_link(pdp, old_path, new_path)

PDATA   *pdp;
char    *old_path;
char    *new_path;

{
    INODE   *ip = dir_path(pdp, old_path, FIND);

    if ( errno )
        return -1;

    if ( (INO_MODE(ip) & IFMT) == IFDIR )  /* don't link to a directory */
    {
        ino_put(ip);
        ERROR(EPERM)
    }

    if ( INO_NLINK(ip) >= MAXLINK )
    {
        ino_put(ip);
        ERROR(EMLINK)
    }

    INO_LINK(ip);
    INO_IS_DIRTY(ip);
    ino_put(ip);

    dir_link(pdp, new_path, INO_DEV(ip), INO_INO(ip));

    if ( errno )
        goto undo;

    return 0;

undo:
    ip = ino_get(INO_DEV(ip), INO_INO(ip));
    INO_UNLINK(ip);
    INO_IS_DIRTY(ip);
    ino_put(ip);

    return -1;
}

/*------------------------------------------------------------------*/

int sys_unlink(pdp, path)

PDATA   *pdp;
char    *path;

{
    INODE   *ip;

    ip  = dir_path(pdp, path, DELETE);

    if ( errno )
        return -1;

    INO_UNLINK(ip);
    INO_SET_CTIME(ip, TIME());
    INO_IS_DIRTY(ip);

    ino_put(ip);

    return 0;
}

/*------------------------------------------------------------------*/

int sys_read(eno, pid, chan, nbyte)

int     eno;
INT     pid;
INT     chan;
INT     nbyte;

{
    ENTRY   *ep  = &ftable[eno];
    INODE   *ip  = ep->inode;
    SHORT   mode = (ep->mode) & 3;
    SHORT   type;
    LONG    offset, size;
    INT     done = 0;
    INT     boff, amnt, count;
    BLKNO   blkno;
    BLKNO   rdahd = (BLKNO)0;

    if ( mode == 1 )            /* write only   */
        ERROR(EACCES)

    type = INO_MODE(ip) & IFMT;

    if ( type == IFIFO )
        return pipe_read(ip, pid, chan, nbyte, ep->mode & O_NDELAY);
    else if ( type == IFCHR || type == IFBLK )
        return raw_read(ep, pid, chan, nbyte);

    offset = ep->offset;

    ip = ino_get(INO_DEV(ip), INO_INO(ip));     /* lock the inode   */

    size = INO_SIZE(ip);

    if ( size - offset < (LONG) nbyte )
        nbyte = (INT) (size - offset);

    while ( done < nbyte )
    {
        if ( rdahd != (BLKNO)0 )
        {
            blkno = rdahd;
            rdahd = (BLKNO)0;
        }
        else
            blkno = ino_map(ip, offset, &boff, &count, &rdahd, FALSE);

        amnt = MIN(count, nbyte - done); 
 
        if ( blkno != (BLKNO) 0 )
            buf_cp_to_msg(INO_DEV(ip), blkno, pid, chan, boff, amnt, rdahd);
        else 
            data_null(pid, chan, amnt);
 
        offset += amnt;
        done   += amnt;
    }

    INO_SET_ATIME(ip, TIME());
    INO_IS_DIRTY(ip);

    ino_put(ip);

    ep->offset = offset;

    return (int) done;
}

/*------------------------------------------------------------------*/

int sys_write(eno, pid, chan, nbyte)

int     eno;
INT     pid;
INT     chan;
INT     nbyte;

{
    ENTRY   *ep  = &ftable[eno];
    INODE   *ip  = ep->inode;
    SHORT   mode = (ep->mode) & 3;
    SHORT   type;
    LONG    offset, now;
    INT     done = 0;
    INT     boff, amnt, count;
    BLKNO   blkno;
    BLKNO   rdahd;              /* a dummy here             */

    if ( mode == 0 )            /* opened read only         */
        ERROR(EACCES)

    if ( INO_ROFS(ip) )         /* read only file system    */
        ERROR(EROFS)

    type = INO_MODE(ip) & IFMT;

    if ( type == IFIFO )
        return pipe_write(ip, pid, chan, nbyte, ep->mode & O_NDELAY);
    else if ( type == IFCHR || type == IFBLK )
        return raw_write(ep, pid, chan, nbyte);

    offset = ep->offset;

    ip = ino_get(INO_DEV(ip), INO_INO(ip));     /* lock the inode   */

    while ( done < nbyte ) 
    {
        blkno = ino_map(ip, offset, &boff, &count, &rdahd, TRUE);

        if ( blkno == (BLKNO)0 )
            break;                  /* no more room in file system  */

        amnt = MIN(count, nbyte - done);

        buf_cp_from_msg(INO_DEV(ip), blkno, pid, chan, boff, amnt, FALSE);

        offset += amnt;
        done   += amnt;
    }

    ep->offset = offset;

    if ( offset > INO_SIZE(ip) )
        INO_SET_SIZE(ip, offset);

    now = TIME();
    INO_SET_ATIME(ip, now);
    INO_SET_MTIME(ip, now);
    INO_IS_DIRTY(ip);

    ino_put(ip);

    return (int) done;
}

/*------------------------------------------------------------------*/

long sys_lseek(eno, offset, dir)

int     eno;
long    offset;
int     dir;

{
    ENTRY   *ep = &ftable[eno];

    switch ( dir )
    {
    case 0:
        if ( offset < 0L )
            LERROR(EINVAL)

        ep->offset = offset;
        break;
    case 1:
        if ( ep->offset + offset < 0L )
            LERROR(EINVAL)

        ep->offset += offset;
        break;
    case 2:
        if ( ep->inode->size + offset < 0L )
            LERROR(EINVAL)

        ep->offset = ep->inode->size + offset;
        break;
    default:
        LERROR(EINVAL)
    }

    if ( ep->offset > ep->inode->size )
    {
        ep->inode->size   = ep->offset;
        ep->inode->flags |= I_DIRTY;
    } 

    return (long) ep->offset;
}
/*------------------------------------------------------------------*/

int sys_access(pdp, path, amode)

PDATA   *pdp;
char    *path;
SHORT   amode;

{
    SHORT   perm;
    INODE   *ip;

    ip = dir_path(pdp, path, FIND);

    if ( errno )
        return -1;

    perm = ino_perms(ip, pdp->uid, pdp->gid);

    ino_put(ip);

    if ( (amode & TWRMODE) && INO_ROFS(ip) )
        ERROR(EROFS)

    if ( (amode & TRDMODE && (perm & 04) == 0) ||
         (amode & TWRMODE && (perm & 02) == 0) )
            ERROR(EACCES)

    return 0;
}

/*------------------------------------------------------------------*/

int sys_chmod(pdp, path, new_mode)

PDATA   *pdp;
char    *path;
SHORT   new_mode; 

{
    INODE   *ip;
    SHORT   mode;

    ip = dir_path(pdp, path, FIND);

    if ( errno )
        return -1;

    if ( INO_ROFS(ip) )
    {
        ino_put(ip);
        ERROR(EROFS)
    }

    if ( pdp->uid != INO_UID(ip) && pdp->uid != ROOT_UID )
    {
        ino_put(ip);
        ERROR(EPERM)
    }

    mode = INO_MODE(ip);
    mode &= IFMT;
    mode |= (new_mode & 0xfff);
    INO_SET_MODE(ip, mode);

    if ( pdp->gid != INO_GID(ip) && pdp->uid != ROOT_UID )  
        INO_SET_MODE(ip, mode &= ~ISGID);

    if ( pdp->uid != ROOT_UID )
        INO_SET_MODE(ip, mode &= ~ISVTX);

    INO_SET_CTIME(ip, TIME());
    INO_IS_DIRTY(ip);

    ino_put(ip);

    return 0;
}

/*------------------------------------------------------------------*/

int sys_chown(pdp, path, new_uid, new_gid)

PDATA   *pdp;
char    *path;
SHORT   new_uid;
SHORT   new_gid; 

{
    INODE   *ip;
    SHORT   mode;

    ip = dir_path(pdp, path, FIND);

    if ( errno )
        return -1;

    if ( INO_ROFS(ip) )
    {
        ino_put(ip);
        ERROR(EROFS)
    }

    if ( pdp->uid != INO_UID(ip) && pdp->uid != ROOT_UID )
    {
        ino_put(ip);
        ERROR(EPERM)
    }

    if ( pdp->uid != ROOT_UID ) 
    {
        mode = INO_MODE(ip);
        INO_SET_MODE(ip, mode & ~(ISUID | ISGID));
    }

    INO_SET_OWNER(ip, new_uid, new_gid);
    INO_SET_CTIME(ip, TIME());
    INO_IS_DIRTY(ip);

    ino_put(ip);

    return 0;
}

/*------------------------------------------------------------------*/

int sys_chdir(pdp, path)

PDATA   *pdp;
char    *path;

{
    INODE   *ip = dir_path(pdp, path, FIND);

    if ( errno )
        return -1;

    if ( INO_DEV(ip) != pdp->cdev || INO_INO(ip) != pdp->cino )
    { 
        sys_unbind(pdp->cdev, pdp->cino);

        pdp->cdev = INO_DEV(ip);
        pdp->cino = INO_INO(ip);
        INO_INCREF(ip);
    }

    ino_put(ip);

    return 0;
}

/*------------------------------------------------------------------*/

int sys_chroot(pdp, path)

PDATA   *pdp;
char    *path;

{
    INODE   *ip = dir_path(pdp, path, FIND);

    if ( errno )
        return -1;

    if ( INO_DEV(ip) != pdp->rdev || INO_INO(ip) != pdp->rino )
    {
        sys_unbind(pdp->rdev, pdp->rino);

        pdp->rdev = INO_DEV(ip);
        pdp->rino = INO_INO(ip);
        INO_INCREF(ip);
    }

    ino_put(ip);

    return 0;
}

/*------------------------------------------------------------------*/

int sys_mknod(pdp, path, perms, dev)

PDATA   *pdp;
char    *path;
SHORT   perms;
DEV     dev;

{
    /* if new node not named pipe and user not super user return error */

    if ( (perms & IFIFO) == 0 && pdp->uid != ROOT_UID )
        ERROR(EPERM) 

    return dir_node(pdp, path, perms, dev);
}

/*------------------------------------------------------------------*/

int sys_mount(pdp, spec, dir, rwflag)

PDATA   *pdp;
char    *spec;
char    *dir;
int     rwflag;

{
    INODE   *sip = dir_path(pdp, spec, FIND);
    INODE   *dip;
    LONG    now;

    if ( errno )
        return -1;

    if ( (INO_MODE(sip) & IFBLK) != IFBLK )
    {
        ino_put(sip);
        ERROR(ENOTBLK)
    }

    dip = dir_path(pdp, dir, FIND);

    if ( errno )
    {
        ino_put(sip);
        return -1;
    }

    if ( (INO_MODE(dip) & IFMT) != IFDIR || INO_REFCNT(dip) != 1 )
    {
        ino_put(sip);
        ino_put(dip);
        if ( INO_REFCNT(dip) != 1 )
            ERROR(EBUSY)
        else
            ERROR(ENOTDIR)
    }

    if ( sup_mount(sip, dip, rwflag) == -1 )
        return -1;

    INO_INCREF(dip);
    now = TIME();
    INO_SET_ATIME(sip, now);
    INO_SET_ATIME(dip, now);
    INO_IS_DIRTY(sip);
    INO_IS_DIRTY(dip);

    ino_put(sip);
    ino_put(dip);

    return 0;
}

/*------------------------------------------------------------------*/

int sys_umount(pdp, spec)

PDATA   *pdp;
char    *spec;

{
    INODE   *ip = dir_path(pdp, spec, FIND);

    if ( errno )
        return -1;

    if ( (INO_MODE(ip) & IFBLK) != IFBLK )
    {
        ino_put(ip);
        ERROR(ENOTBLK)
    }

    buf_sync();
    sup_sync();

    INO_SET_ATIME(ip, TIME());
    INO_IS_DIRTY(ip);

    return sup_umount(ip);
}

/*------------------------------------------------------------------*/

int sys_stat(pdp, path, stbuf)

PDATA       *pdp;
char        *path;
struct stat *stbuf;

{
    INODE   *ip = dir_path(pdp, path, FIND);

    if ( errno )
        return -1;

    ino_stat(ip, stbuf);

    ino_put(ip);

    return 0;
}

/*------------------------------------------------------------------*/

void sys_fstat(eno, stbuf)

int         eno;
struct stat *stbuf;

{
    ENTRY   *ep  = &ftable[eno];

    ino_stat(ep->inode, stbuf);
}

/*------------------------------------------------------------------*/

int sys_pipe(pr_eno, pw_eno)

int     *pr_eno;
int     *pw_eno;

{
    INODE   *ip = sup_ialloc((DEV)0);
    int     r_eno, w_eno; 
    LONG    now;

    if ( (r_eno = new_entry(ip, 0L, O_RDONLY)) == -1 )
        return -1;

    if ( (w_eno = new_entry(ip, 0L, O_WRONLY)) == -1 )
    {
        ftable[r_eno].inode = NIL_INODE;        /* give it back */
        return -1;
    }

    INO_SET_MODE(ip, IFIFO | 0600);
    now = TIME();
    INO_SET_ATIME(ip, now);
    INO_SET_MTIME(ip, now);
    INO_SET_CTIME(ip, now);
    INO_INCREF(ip);
    INO_INCREF(ip);
    INO_INCREF(ip);             /* ino_put decrements once  */
    INO_INCRDRS(ip);
    INO_INCWRTS(ip);
    INO_SET_IN(ip, 0L);
    INO_SET_OUT(ip, 0L);
    INO_IS_DIRTY(ip);

    ino_put(ip);

    *pr_eno = r_eno; 
    *pw_eno = w_eno; 
 
    return 0;
}

/*------------------------------------------------------------------*/

void sys_sync()

{
    buf_sync();
    sup_sync();
}

/*------------------------------------------------------------------*/

int sys_fcntl(eno, cmd, arg)

int     eno;
int     cmd;
INT     arg;

{
    ENTRY   *ep  = &ftable[eno];

    switch (cmd)
    {
    case F_SETFL:
        ep->mode |= ( arg & (O_NDELAY | O_APPEND) );
        break;
    case F_GETFL:
        return (int) ep->mode;
    default:
        ERROR(EINVAL)
    }

    return 0;
}

/*------------------------------------------------------------------*/

int sys_ustat(dev, ubufp)

DEV             dev;
struct ustat    *ubufp;

{
    return sup_ustat(dev, ubufp);
}

/*------------------------------------------------------------------*/

int sys_utime(pdp, path, axtime, modtime)

PDATA   *pdp;
char    *path;
long    axtime;
long    modtime;

{
    INODE   *ip = dir_path(pdp, path, FIND);
    SHORT   perm;
    LONG    now;

    if ( errno )
        return -1;

    if ( pdp->uid != ROOT_UID && pdp->uid != INO_UID(ip) ) 
        errno = EPERM;

    if ( errno == 0 && axtime == 0L )
    {
        perm = ino_perms(ip, pdp->uid, pdp->gid); 
  
        if ( (perm & 02) )
        {
            now = TIME();
            INO_SET_ATIME(ip, now);
            INO_SET_MTIME(ip, now);
            INO_IS_DIRTY(ip);
        }
        else
            errno = EACCES;
    }
    else if ( errno == 0 )
    {
        INO_SET_ATIME(ip, axtime);
        INO_SET_MTIME(ip, modtime);
        INO_IS_DIRTY(ip);
    }

    ino_put(ip);

    return ( errno ) ? -1 : 0;
} 

/*------------------------------------------------------------------*/

int sys_ioctl(eno, request, arg, szp)

int     eno;
int     request;
char    *arg;
INT     *szp;

{
    ENTRY   *ep  = &ftable[eno];
    INODE   *ip  = ep->inode;
    DEV     dev  = (DEV) INO_SIZE(ip);

    *szp = 0;               /* to be on the safe side   */

    if ( (INO_MODE(ip) & IFMT) != IFCHR )
        ERROR(ENOTTY)

    *szp = dev_ioctl(dev, request, arg);

    return ( errno ) ? -1 : 0;
} 

/*------------------------------------------------------------------*/

void sys_bind(dev, ino)

DEV     dev;
SHORT   ino;

{
    INODE *ip = ino_get(dev, ino);

    INO_INCREF(ip);
    ino_put(ip);
}

/*------------------------------------------------------------------*/

void sys_unbind(dev, ino)

DEV     dev;
SHORT   ino;

{
    INODE *ip = ino_get(dev, ino);

    INO_DECREF(ip);
    ino_put(ip);
}

/*------------------------------------------------------------------*/

void sys_incref(eno)

int eno;

{
    ++ftable[eno].refcnt;
}

/*------------ Function used by the Proceses Manager ---------------*/ 

int sys_exec(pdp, path, devp, inop)

PDATA   *pdp;
char    *path;
DEV     *devp;
SHORT   *inop;

{
    INODE   *ip  = dir_path(pdp, path, FIND);

    if ( errno != 0 )
        return -1;

    if ( (INO_MODE(ip) & IFMT) != IFREG  ||
         (ino_perms(ip, pdp->uid, pdp->gid) & 01) == 0 )
    {                       /* not allowed to execute   */
        ino_put(ip);
        ERROR(EACCES)
    }

    *devp = INO_DEV(ip); 
    *inop = INO_INO(ip); 
 
    INO_INCREF(ip);
 
    INO_SET_ATIME(ip, TIME());
    INO_IS_DIRTY(ip);

    ino_put(ip);

    return 0;
}

/*------------------------------------------------------------------*/

int sys_ahdr(dev, ino, ahdrp)

DEV     dev;
SHORT   ino;
AHDR    *ahdrp;

{
    INODE   *ip = ino_get(dev, ino);
    INT     tot_data;
    char    buf[256];
    struct xexec *xexecp;
    struct xext  *xextp;
    struct xseg  *xsegp;

    buf_copyin(dev, INO_FSTBLK(ip), buf, 0, 256, (BLKNO)0);

    ino_put(ip); 
 
    xexecp = (struct xexec*) buf;

    tot_data = 1 + (INT) ((xexecp->x_data + xexecp->x_bss - 1L) / 4096);

    ahdrp->size_text = 1 + (INT) ((xexecp->x_text - 1L) / 4096L);
    ahdrp->size_data = 1 + (INT) ((xexecp->x_data - 1L) / 4096L);
    ahdrp->size_bss  = tot_data - ahdrp->size_data;

    xextp = (struct xext*) (buf + sizeof(struct xexec));
    xsegp = (struct xseg*) (buf + xextp->xe_segpos);

    ahdrp->fblk_text = (BLKNO) (xsegp->xs_filpos / 1024L);
    ahdrp->vadr_text = (ADDR) (xsegp->xs_rbase);

    ++xsegp;

    ahdrp->fblk_data = (BLKNO) (xsegp->xs_filpos / 1024L);
    ahdrp->vadr_data = (ADDR) (xsegp->xs_rbase);

    ++xsegp;

    ahdrp->vadr_bss  = (ADDR) (xsegp->xs_rbase);
    
    return 0;
}

/*------------ Function used by the Memory Manager -----------------*/ 

void sys_getblks(dev, blkp, chan)

DEV     dev;
BLKNO   *blkp;
INT     chan;

{
    int     i;
    BLKNO   rdahd;

    for ( i = 0; i < FACTOR; ++i )
    {
        rdahd = ( i < FACTOR - 1 ) ? blkp[i + 1] : (BLKNO)0;

        if ( blkp[i] != (BLKNO)0 )
            buf_cp_to_msg(dev, blkp[i], MM_PID, chan, 0, BLOCK_SIZE, rdahd); 
        else
            data_null(MM_PID, chan, BLOCK_SIZE);
    }
}

/*------------------------------------------------------------------*/

void sys_blknos(dev, ino, fst_blk, nblks, chan)

DEV     dev;
SHORT   ino;
BLKNO   fst_blk;
INT     nblks;
INT     chan;

{
    INODE   *ip = ino_get(dev, ino);

    ino_blknos(ip, fst_blk, nblks, chan);

    ino_put(ip);
}

/*------------ Local Function Definitions --------------------------*/ 

static int new_entry(ino, offset, mode)

INODE   *ino;
LONG    offset;
SHORT   mode;

{
    ENTRY   *ep;

    for ( ep = ftable; ep < ftable + TABSIZE; ++ep )
        if ( ep->inode == NIL_INODE )
            break;

    if ( ep >= ftable + TABSIZE )
        ERROR(ENFILE)

    ep->inode  = ino;
    ep->offset = offset;
    ep->mode   = mode;
    ep->refcnt = 1;

    return (ep - ftable);
}

/*------------------------------------------------------------------*/

static int pipe_read(ip, pid, chan, nbyte, no_wait)

INODE   *ip;
INT     pid;
INT     chan;
INT     nbyte;
SHORT   no_wait;

{
    LONG    offset;
    LONG    size;
    INT     total = 0;
    INT     amnt  = 0;
    INT     now, done;
    INT     boff, count;
    BLKNO   blkno;
    BLKNO   rdahd = (BLKNO)0;

    while ( total < nbyte )
    {
        now = MIN(nbyte - total, MAXPIPE);
 
        for (;;)
        {
            if ( INO_SIZE(ip) )
            {
                now = (INT) (MIN((LONG)now, INO_SIZE(ip)));
                ip  = ino_get(INO_DEV(ip), INO_INO(ip)); /* lock the inode  */
                break;
            }

            if ( INO_WRTS(ip) == 0 || total > 0 || no_wait ) 
                return (int) total;         /* no point waiting */ 
 
            sleep((EVENT)&INO_SIZE(ip)); 
        }

        offset = INO_OUT(ip);

        for ( done = 0; done < now; done += amnt ) 
        {
            if ( rdahd != (BLKNO)0 )
            {
                blkno = rdahd;
                rdahd = (BLKNO)0;
            }
            else
                blkno = ino_map(ip, offset, &boff, &count, &rdahd, FALSE);

            amnt = MIN(count, now - done);

            buf_cp_to_msg(INO_DEV(ip), blkno, pid, chan, boff, amnt, rdahd);

            offset += amnt;

            if ( offset >= MAXPIPE )
                offset -= MAXPIPE;
        }

        INO_SET_OUT(ip, offset);
        size = INO_SIZE(ip) - done; 
        INO_SET_SIZE(ip, size);
        total += done;
 
        wakeup((EVENT)&INO_SIZE(ip));

        INO_SET_ATIME(ip, TIME());
        INO_IS_DIRTY(ip);
        ino_put(ip);
    }

    return (int) total;
}

/*------------------------------------------------------------------*/

static int pipe_write(ip, pid, chan, nbyte, no_wait)

INODE   *ip;
INT     pid;
INT     chan; 
INT     nbyte;
SHORT   no_wait;

{
    LONG    offset;
    LONG    size;
    INT     total = 0;
    INT     amnt  = 0;
    INT     now, done;
    INT     boff, count;
    BLKNO   blkno;
    BLKNO   rdahd;              /* a dummy here */

    while ( total < nbyte )
    {
        now = MIN(nbyte - total, MAXPIPE);
 
        for (;;)
        {
            if ( INO_RDRS(ip) == 0 )
                ERROR(EPIPE)

            if ( now <= MAXPIPE - INO_SIZE(ip) )
            {
                ip = ino_get(INO_DEV(ip), INO_INO(ip)); /* lock the inode   */
                break;
            }

            if ( no_wait )
                return 0;

            sleep((EVENT)&INO_SIZE(ip)); 
        }

        offset = INO_IN(ip);

        for ( done = 0; done < now; done += amnt ) 
        {
            blkno = ino_map(ip, offset, &boff, &count, &rdahd, TRUE);

            if ( blkno == (BLKNO)0 )
            {
                ino_put(ip);
                return -1;          /* no more room in file system  */
            }

            amnt = MIN(count, now - done);

            buf_cp_from_msg(INO_DEV(ip), blkno, pid, chan, boff, amnt, FALSE);

            offset += amnt;

            if ( offset >= MAXPIPE )
                offset -= MAXPIPE;
        }

        INO_SET_IN(ip, offset);
        size = INO_SIZE(ip) + done;
        INO_SET_SIZE(ip, size);
        total += done;
 
        wakeup((EVENT)&INO_SIZE(ip));

        INO_SET_ATIME(ip, TIME());
        INO_IS_DIRTY(ip);
        ino_put(ip);
    }

    return (int) total;
}

/*------------------------------------------------------------------*/

static int raw_read(ep, pid, chan, nbyte)

ENTRY   *ep;
INT     pid;
INT     chan;
INT     nbyte;

{
    INODE   *ip = ep->inode;
    DEV     dev = (DEV) INO_SIZE(ip);
    int     n;

    n = dev_rread(dev, pid, chan, ep->offset, nbyte);

    if ( n > 0 )
        ep->offset += (LONG) n;

    return n;
}

/*------------------------------------------------------------------*/

static int raw_write(ep, pid, chan, nbyte)

ENTRY   *ep;
INT     pid;
INT     chan;
INT     nbyte;

{
    INODE   *ip = ep->inode;
    DEV     dev = (DEV) INO_SIZE(ip);
    int     n;

    n = dev_rwrite(dev, pid, chan, ep->offset, nbyte);

    if ( n > 0 )
        ep->offset += (LONG) n;

    return n; 
}

