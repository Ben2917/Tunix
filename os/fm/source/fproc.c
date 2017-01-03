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
* This module manages the information about active processes.
* In particular it handles the per process file tables.
* It knows nothing about file system primitives such as inodes
* and buffers.
*
***********************************************************
*/

#include    "../include/ktypes.h" 
#include    "../include/macros.h"
#include    "../include/pdata.h"
#include    "../include/sys.h" 
#include    "../include/fproc.h"
#include    "../include/fcntl.h"
#include    "../include/errno.h"
#include    "../include/stat.h"
#include    "../include/ustat.h"
#include    "../include/pid.h"
#include    "../include/ahdr.h"
#include    "../include/fid.h"

#define N_FILES     20      /* max. no. of files proc. can open */
#define N_PROCS     128     /* max. no. of processes active     */

#define BLK_SIZE    4096    /* size of transfer buffer          */

extern int  errno;

typedef struct
{
    INT     pid;                /* identifier for owner process     */
    DEV     dev;                /* device of executable binary      */
    SHORT   ino;                /* inode no. of   "       "         */
    PDATA   pdata;              /* uid, gid, root, cdir             */
    SHORT   umask;              /* mask for file creation           */
    int     ft_indx[N_FILES];   /* index of entry in file table     */
    int     cl_exec[N_FILES];   /* close on exec if TRUE            */

} ENTRY;

#define ERROR(errnr)        { errno = errnr; return -1; }
#define NIL_ENTRY   (ENTRY *)0
#define NIL_DATA    (PDATA *)0

static ENTRY procs[N_PROCS];
    
static ENTRY    *find_entry();
static PDATA    *find_data();

/*------------------------------------------------------*/

void proc_init()

{
    ENTRY   *ep;
    PDATA   *pdp;
    int     i, j;

    sys_init(); 
     
    /* create file tables for Memory Manager, Process Manager and init  */

    for ( j = 0, ep = procs; j < 3; ++j, ++ep )
    {
        pdp       = &ep->pdata;
        ep->umask = 022;
        ep->ino   = 0;
        pdp->uid  = ROOT_UID;
        pdp->gid  = ROOT_GID;

        pdp->rdev = pdp->cdev = ROOT_DEV;
        pdp->rino = pdp->cino = ROOT_INO;

        sys_bind(ROOT_DEV, ROOT_INO);
        sys_bind(ROOT_DEV, ROOT_INO);

        for ( i = 0; i < N_FILES; ++i )
            ep->ft_indx[i] = -1;
    }
  
    procs[0].pid = MM_PID;
    procs[1].pid = PM_PID;
    procs[2].pid = INIT_PID;

    for ( ; ep < procs + N_PROCS; ++ep ) 
        ep->pid = 0; 
} 

/*------------------------------------------------------*/ 

int proc_sync(pid)

INT pid;

{
    PDATA   *pdp = find_data(pid);

    if ( pid != FM_PID )
    {
        if ( pdp == NIL_DATA )
            ERROR(EINVAL)

        if ( pdp->uid != procs[0].pdata.uid )
            ERROR(EACCES)
    }

    sys_sync();

    return 0;
}

/*------------------------------------------------------*/

int proc_umask(pid, new_mask)

INT     pid;
SHORT   new_mask;

{
    ENTRY   *ep = find_entry(pid);
    int     old_mask = (int) ep->umask;

    if ( ep != NIL_ENTRY )
        ep->umask = new_mask;
 
    return old_mask; 
}

/*------------------------------------------------------*/

int proc_chmod(pid, path, mode)

INT     pid;
char    *path;
SHORT   mode;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_chmod(pdp, path, mode); 
}

/*------------------------------------------------------*/

int proc_chown(pid, path, owner, group)

INT     pid;
char    *path;
SHORT   owner;
SHORT   group;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_chown(pdp, path, owner, group); 
}

/*------------------------------------------------------*/

int proc_chdir(pid, path)

INT     pid;
char    *path;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_chdir(pdp, path);
}

/*------------------------------------------------------*/

int proc_chroot(pid, path)

INT     pid;
char    *path;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_chroot(pdp, path);
}

/*------------------------------------------------------*/

int proc_open(pid, path, mode, perms)

INT     pid;
char    *path;
SHORT   mode;
SHORT   perms;

{
    ENTRY   *ep = find_entry(pid);
    PDATA   *pdp = &ep->pdata; 
    int     fd; 

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    for ( fd = 0; fd < N_FILES; ++fd )
        if ( ep->ft_indx[fd] == -1 )
            break;

    if ( fd >= N_FILES )
        ERROR(EMFILE)

    perms &= ~(ep->umask);

    ep->cl_exec[fd] = FALSE;

    if ( (ep->ft_indx[fd] = sys_open(pdp, path, mode, perms)) == -1 )
        return -1;
    else
        return fd;
}

/*------------------------------------------------------*/

int proc_dup(pid, old_fd)

INT     pid;
int     old_fd;

{
    ENTRY   *ep = find_entry(pid);
    int     fd;

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( old_fd < 0 || old_fd >= N_FILES || ep->ft_indx[old_fd] == - 1)
        ERROR(EBADF)

    for ( fd = 0; fd < N_FILES; ++fd )
        if ( ep->ft_indx[fd] == -1 )
            break;

    if ( fd >= N_FILES )
        ERROR(ENFILE)

    ep->ft_indx[fd] = ep->ft_indx[old_fd];

    sys_incref(ep->ft_indx[fd]);

    return fd;
}

/*------------------------------------------------------*/

int proc_close(pid, fd)

INT     pid;
int     fd;

{
    ENTRY   *ep = find_entry(pid);

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( fd < 0 || fd >= N_FILES )
        ERROR(EBADF)

    if ( ep->ft_indx[fd] == -1 )
        return 0;

    sys_close(ep->ft_indx[fd]);

    ep->ft_indx[fd] = -1;

    return 0;
}

/*------------------------------------------------------*/

long proc_lseek(pid, fd, offset, dir)

INT     pid;
int     fd;
long    offset;
int     dir;

{
    ENTRY   *ep = find_entry(pid);

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( fd < 0 || fd >= N_FILES || ep->ft_indx[fd] == -1 )
        ERROR(EBADF)

    return sys_lseek(ep->ft_indx[fd], offset, dir);
}

/*------------------------------------------------------*/

int proc_read(pid, fd, nbytes)

INT     pid;
int     fd;
INT     nbytes;

{
    ENTRY   *ep = find_entry(pid);

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( fd < 0 || fd >= N_FILES || ep->ft_indx[fd] == -1 )
        ERROR(EBADF)

    return sys_read(ep->ft_indx[fd], pid, STD_CHAN, nbytes);
}

/*------------------------------------------------------*/

int proc_write(pid, fd, nbytes)

INT     pid;
int     fd;
INT     nbytes;

{
    ENTRY   *ep = find_entry(pid);

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( fd < 0 || fd >= N_FILES || ep->ft_indx[fd] == -1 )
        ERROR(EBADF)

    return sys_write(ep->ft_indx[fd], pid, STD_CHAN, nbytes);
}

/*------------------------------------------------------*/

int proc_link(pid, old_path, new_path)

INT     pid;
char    *old_path;
char    *new_path;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_link(pdp, old_path, new_path);
}

/*------------------------------------------------------*/

int proc_unlink(pid, path)

INT     pid;
char    *path;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_unlink(pdp, path);
}

/*------------------------------------------------------*/

int proc_mknod(pid, path, perms, dev)

INT     pid;
char    *path;
SHORT   perms;
DEV     dev;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_mknod(pdp, path, perms, dev);
}

/*------------------------------------------------------*/

int proc_stat(pid, path, stbuf)

INT         pid;
char        *path;
struct stat *stbuf;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_stat(pdp, path, stbuf);
}

/*------------------------------------------------------*/

int proc_fstat(pid, fd, stbuf)

INT         pid;
int         fd;
struct stat *stbuf;

{
    ENTRY   *ep = find_entry(pid);

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( fd < 0 || fd >= N_FILES || ep->ft_indx[fd] == -1 )
        ERROR(EBADF)

    sys_fstat(ep->ft_indx[fd], stbuf);

    return 0;
}

/*------------------------------------------------------*/

int proc_access(pid, path, amode)

INT     pid;
char    *path;
SHORT   amode;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_access(pdp, path, amode);
}

/*------------------------------------------------------*/

int proc_mount(pid, spec, dir, rwflag)

INT     pid;
char    *spec;
char    *dir;
int     rwflag;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    if ( pdp->uid != ROOT_UID )
        ERROR(EPERM)

    return sys_mount(pdp, spec, dir, rwflag);
}

/*------------------------------------------------------*/

int proc_umount(pid, spec)

INT     pid;
char    *spec;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    if ( pdp->uid != ROOT_UID )
        ERROR(EPERM)

    return sys_umount(pdp, spec);
}

/*------------------------------------------------------*/

int proc_pipe(pid, pfd)

INT     pid;
int     *pfd;

{
    ENTRY   *ep = find_entry(pid);
    int     fd0, fd1, r_indx, w_indx;

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    for ( fd0 = 0; fd0 < N_FILES; ++fd0 )   
        if ( ep->ft_indx[fd0] == -1 )
            break;

    if ( fd0 >= N_FILES )
        ERROR(EMFILE)

    for ( fd1 = fd0 + 1; fd1 < N_FILES; ++fd1 ) 
        if ( ep->ft_indx[fd1] == -1 ) 
            break; 
 
    if ( fd1 >= N_FILES ) 
        ERROR(EMFILE) 
 
    if ( sys_pipe(&r_indx, &w_indx) == -1 ) 
        return -1; 
 
    ep->ft_indx[fd0] = r_indx;
    *pfd++           = fd0;

    ep->ft_indx[fd1] = w_indx;
    *pfd             = fd1;

    return 0;
}

/*------------------------------------------------------*/

int proc_fcntl(pid, fd, cmd, arg)

INT     pid;
int     fd;
int     cmd;
INT     arg;

{
    ENTRY   *ep = find_entry(pid);
    INT     i;

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( fd < 0 || fd >= N_FILES || ep->ft_indx[fd] == -1 )
        ERROR(EBADF)

    switch ( cmd )
    {
    case F_DUPFD:
        if ( arg >= N_FILES )
            ERROR(EINVAL)

        for ( i = arg; i < N_FILES; ++i )
            if ( ep->ft_indx[i] == -1 )
                break;

        if ( i >= N_FILES )
            ERROR(EMFILE)

        ep->ft_indx[i]  = ep->ft_indx[fd];
        ep->cl_exec[i] = FALSE;

        sys_incref(ep->ft_indx[fd]);

        return 0;
    case F_SETFD:
        ep->cl_exec[fd] = (int) (arg & 1);
        return 0;
    case F_GETFD:
        return ep->cl_exec[fd];
    default:
        return sys_fcntl(ep->ft_indx[fd], cmd, arg); 
    }
}

/*------------------------------------------------------*/

int proc_ustat(dev, ubufp)

DEV             dev;
struct ustat    *ubufp;

{
    return sys_ustat(dev, ubufp);
}

/*------------------------------------------------------*/

int proc_utime(pid, path, axtime, modtime)

INT     pid;
char    *path;
long    axtime;
long    modtime;

{
    PDATA   *pdp = find_data(pid); 

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    return sys_utime(pdp, path, axtime, modtime);
}

/*------------------------------------------------------*/

int proc_ioctl(pid, fd, request, arg, szp)

INT     pid;
int     fd;
int     request;
char    *arg;
INT     *szp;

{
    ENTRY   *ep = find_entry(pid);

    *szp = 0;               /* purely precautionary */

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    if ( fd < 0 || fd >= N_FILES || ep->ft_indx[fd] == -1 )
        ERROR(EBADF)

    return sys_ioctl(ep->ft_indx[fd], request, arg, szp);
}

/*------------------ Functions used by Process Manager --------------*/

int proc_exec(path, pid, fidp)

char        *path;
INT         pid;
LONG        *fidp;

{
    ENTRY   *ep  = find_entry(pid);
    PDATA   *pdp = &ep->pdata;
    int     i, res;

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)

    for ( i = 0; i < N_FILES; ++i )
        if ( ep->ft_indx[i] != -1 && ep->cl_exec[i] )
            sys_close(ep->ft_indx[i]);

    if ( ep->ino )
        sys_unbind(ep->dev, ep->ino);

    res = sys_exec(pdp, path, &(ep->dev), &(ep->ino));

    *fidp = FID(ep->dev, ep->ino);

    return res;
}

/*------------------------------------------------------*/

void proc_fork(pid, ppid)

INT     pid;
INT     ppid;

{
    ENTRY   *pep = find_entry(ppid);
    PDATA   *pdp = &pep->pdata; 
    ENTRY   *ep;
    int     *ftp;

    for ( ep = procs; ep < procs + N_PROCS; ++ep )
        if ( ep->pid == 0 )
            break;

    if ( pep == NIL_ENTRY || ep >= procs + N_PROCS )
        return; 

    (void) MEMCPY(ep, pep, sizeof(ENTRY));

    ep->pid = pid;

    for ( ftp = ep->ft_indx; ftp < ep->ft_indx +  N_FILES; ++ftp )
        if ( *ftp != -1 )
            sys_incref(*ftp);

    sys_bind(pdp->rdev, pdp->rino);
    sys_bind(pdp->cdev, pdp->cino);

    if ( ep->ino )
        sys_bind(ep->dev, ep->ino);
}

/*------------------------------------------------------*/

int proc_seteid(pid, new_uid, new_gid)

INT     pid;
SHORT   new_uid;
SHORT   new_gid;

{
    PDATA   *pdp = find_data(pid);

    if ( pdp == NIL_DATA )
        ERROR(EINVAL)
                
    pdp->uid = new_uid;
    pdp->gid = new_gid;

    return 0;
}

/*------------------------------------------------------*/ 

void proc_exit(pid)

INT     pid;

{
    ENTRY   *ep  = find_entry(pid);
    PDATA   *pdp = &ep->pdata;
    int     *ftp;

    if ( ep != NIL_ENTRY )
    {
        for ( ftp = ep->ft_indx; ftp < ep->ft_indx + N_FILES; ++ftp )
            if ( *ftp != -1 )
                sys_close(*ftp);

        sys_unbind(pdp->rdev, pdp->rino);
        sys_unbind(pdp->cdev, pdp->cino);
  
        ep->pid = 0; 
    }

    if ( ep->ino )
        sys_unbind(ep->dev, ep->ino);
}

/*------------------------------------------------------*/

int proc_ahdr(pid, ahdrp)

INT     pid;
AHDR    *ahdrp;

{
    ENTRY   *ep = find_entry(pid);

    return sys_ahdr(ep->dev, ep->ino, ahdrp);
}

/*------------------ Functions used by Memory Manager ---------------*/

int proc_getblks(dev, blkp, chan)

DEV     dev;
BLKNO   *blkp;
INT     chan;

{
    ENTRY   *ep = find_entry(MM_PID);

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    sys_getblks(dev, blkp, chan);

    return 0;
}

/*------------------------------------------------------*/

int proc_putblks(pid, fd, chan)

INT     pid;
int     fd;
INT     chan;

{
    ENTRY   *ep = find_entry(pid);

    if ( ep == NIL_ENTRY )
        ERROR(EINVAL)

    return sys_write(ep->ft_indx[fd], pid, chan, BLK_SIZE); 
}

/*------------------------------------------------------*/                    

int proc_blknos(fid, fst_blk, nblks, chan)

LONG    fid;
BLKNO   fst_blk;
INT     nblks;
INT     chan;

{
    sys_blknos(F_DEV(fid), F_INO(fid), fst_blk, nblks, chan);
 
    return 0; 
}

/*------------------ Local Function Definitions ---------------------*/

static ENTRY *find_entry(pid)

INT pid;

{
    ENTRY   *ep;

    for ( ep = procs; ep < procs + N_PROCS; ++ep )
        if ( ep->pid == pid )
            return ep;

    return NIL_ENTRY;
}

/*------------------------------------------------------*/

static PDATA *find_data(pid)

INT pid;

{
    ENTRY   *ep;

    for ( ep = procs; ep < procs + N_PROCS; ++ep )
        if ( ep->pid == pid )
            return &(ep->pdata);

    return NIL_DATA;
}
