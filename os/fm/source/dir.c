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
* This is the only module that knows how directories look
* and what paths really mean. Everywhere else a "path" is
* merely a string that somehow indentifies an inode.
*
***********************************************************
*/

#include    "../include/buffer.h"
#include    "../include/pdata.h" 
#include    "../include/inode.h"
#include    "../include/super.h"
#include    "../include/dir.h"
#include    "../include/errno.h"
#include    "../include/macros.h"

typedef struct
{
    SHORT   ino;
    char    name[14];

} DIR_ENTRY;

#define DIR_SIZE    sizeof(DIR_ENTRY)
 
extern int  errno;

/*------------------- Local Data Declarations --------------*/

static char dot[]    = ".             ";
static char dotdot[] = "..            ";

/*------------------- Local Function Declarations ----------*/

static INODE    *dir_descend();
static int      dir_chk();
static SHORT    dir_search();
static INODE    *dir_rm();
static INODE    *dir_create();
static void     new_link();
static int      new_node();
static void     last_component();

#define ERROR(err)  { errno = err; return -1; }
#define IERROR(err) { errno = err; return NIL_INODE; }

/*------------------- Public Functions ---------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: dir_path                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pdp points to a structure defining a process (root,   */
/*    cdir, uid, gid).                                      */
/* 2) path points to a valid path string to be converted    */
/*    into a valid inode.                                   */
/*                                                          */
/* Exit conditions:                                         */
/* 1) On exit EITHER errno == 0 and the return value is a   */
/*    pointer to a locked in-core inode belonging to the    */
/*    last component in path OR errno != 0.                 */
/* 2) If op == FIND and the last component of the path was  */
/*    not found, then an appropriate error (ENOENT,         */
/*    ENOTDIR, ... ) is reported.                           */
/* 3) If op == CREATE and the last component was not found  */
/*    but the next to last component was found (the         */
/*    "parent"), then the first free slot of the "parent"   */
/*    component is filled in with the name of the last      */
/*    component, a new disk inode is assigned (sup_ialloc)  */
/*    and its number is also entered in the appropriate     */
/*    slot. The return value of dir_path points to the      */
/*    in-core inode of this new inode.                      */ 
/* 4) If the last component is found and op == DELETE,      */
/*    then the inode number of the corresponding entry is   */
/*    set to 0.                                             */
/* 5) If op == DELETE and the last component is a direc-    */
/*    tory, then in addition the nlink field of the parent  */
/*    is decremented (because of "..") and the nlink field  */
/*    of the child as well (because of ".").                */
/*                                                          */
/************************************************************/


INODE *dir_path(pdp, path, op)

PDATA   *pdp;
char    *path;
int     op;                 /* what to do if found (or not found)   */

{
    SHORT   ino; 
    LONG    offset; 
    INODE   *ip = dir_descend(pdp, path, &ino, &offset);
    INODE   *iip;
    char    name[14];
    int     is_dir;

    if ( errno )
        return NIL_INODE;

    if ( op == FIND )
    {
        ino_put(ip);  
 
        if ( ino == 0 )         /* last component not found     */
            IERROR(ENOENT)

        ip = ino_get(INO_DEV(ip), ino);

        if ( INO_MNTPNT(ip) )
        {
            iip = sup_mounted(ip);
            ino_put(ip);
            ip = ino_get(INO_DEV(iip), INO_INO(iip));
        }

        return ip;
    }

    if ( op == DELETE )
    {
        if ( ino == 0 )
            errno = ENOENT;             /* last component not found */
        else if ( ino == 1 )
            errno = EACCES;             /* can't rm / or . or ..        */
        else if ( (ino_perms(ip, pdp->uid, pdp->gid) & 2) == 0 )
            errno = EACCES;             /* write not allowed            */
        else if ( dir_chk(INO_DEV(ip), ino, &is_dir) == -1 )
            errno = ENOTDIR;            /* possibly not empty           */
        else if ( is_dir && pdp->uid != ROOT_UID )
            errno = EPERM;              /* only super user can rm a dir */
        else if ( INO_ROFS(ip) )
            errno = EROFS;              /* read only file system        */

        if ( errno )
        {
            ino_put(ip);
            return NIL_INODE;
        }

        return dir_rm(ip, offset, is_dir);
    }

    if ( ino )                  /* op == CREATE but entry found */
    {
        ino_put(ip);

        return ino_get(INO_DEV(ip), ino);
    }

    /* we only get here if last component missing and op == CREATE  */

    if ( (ino_perms(ip, pdp->uid, pdp->gid) & 2) == 0 ) 
        errno = EACCES;
    else if ( INO_ROFS(ip) ) 
        errno = EROFS; 

    if ( errno )
    {
        ino_put(ip);
        return NIL_INODE;
    }

    last_component(path, name);

    return dir_create(ip, offset, name, pdp->uid, pdp->gid);
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: dir_link                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pdp points to a structure defining a process (root,   */
/*    cdir, uid, gid).                                      */
/* 2) new_path points to a valid path string to be linked   */
/*    to an existing inode.                                 */
/* 3) dev is the device number of this inode.               */
/* 4) And oino is its inode number ("old inode").           */
/*                                                          */
/* Exit conditions:                                         */
/* 1) If the link is successful, errno == 0; otherwise it   */
/*    contains the correct error code.                      */
/* 2) In case of success a new directory entry in the       */
/*    "parent" directory of new_path is created with the    */
/*    last component of new_path as name and the old inode  */
/*    number as inode number. The parent inode is updated   */
/*    and released.                                         */
/* 3) The nlink field of the old inode is NOT altered.      */
/*                                                          */
/************************************************************/

void dir_link(pdp, new_path, dev, oino)

PDATA   *pdp;
char    *new_path;
DEV     dev;
SHORT   oino;

{
    SHORT   ino; 
    LONG    offset; 
    INODE   *ip = dir_descend(pdp, new_path, &ino, &offset);
    char    name[14];

    if ( errno )
        return;

    if ( INO_ROFS(ip) )     /* read only file system    */
        errno = EROFS;
    else if ( ino )         /* new_path already exists  */ 
        errno = EEXIST; 
    else if ( INO_DEV(ip) != dev ) 
        errno = EXDEV; 
    else if ( (ino_perms(ip, pdp->uid, pdp->gid) & 2) == 0 ) 
        errno = EACCES; 
 
    if ( errno )
    {
        ino_put(ip);
        return;
    }

    last_component(new_path, name);

    new_link(ip, offset, oino, name);

    ino_put(ip);
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: dir_node                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pdp points to a structure defining a process (root,   */
/*    cdir, uid, gid).                                      */
/* 2) path points to a valid path string to be converted    */
/*    into a new inode.                                     */
/* 3) perms becomes the mode field of the new inode.        */
/* 4) dev is the device number of the device this inode is  */
/*    to describe if (perms & IFMT) == IFCHR or IFBLK.      */
/*                                                          */
/* Exit conditions:                                         */
/* 1) If the new inode is successfully created, errno == 0  */
/*    and the return value is 0. Otherwise errno contains   */
/*    the correct error code and the return value is -1.    */
/* 2) All inodes have been updated and released.            */
/* 3) A corresponding entry in the "parent" directory of    */
/*    path has been made and filled in with the number of   */
/*    the new inode and the last component of path as name. */
/*                                                          */
/************************************************************/
 
int dir_node(pdp, path, perms, dev)

PDATA   *pdp;
char    *path;
SHORT   perms;
DEV     dev;

{
    SHORT   ino; 
    LONG    offset; 
    INODE   *ip = dir_descend(pdp, path, &ino, &offset);
    char    name[14];

    if ( errno )
        return -1;

    if ( ino )          /* path already exists  */
        errno = EEXIST;
    else if ( INO_ROFS(ip) )
        errno = EROFS;
    else if ( (ino_perms(ip, pdp->uid, pdp->gid) & 2) == 0 ) 
        errno = EACCES;

    if ( errno )
    {
        ino_put(ip);
        return -1;
    }

    last_component(path, name);

    return new_node(ip, offset, name, perms, dev, pdp->uid, pdp->gid);
}

/*------------------ Local function definitions ------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: dir_search                                     */
/*                                                          */
/* Entry conditions:                                        */
/* 1) ip points to a locked in-core inode of a directory.   */
/* 2) name points to a string of exactly 14 characters, not */
/*    0-terminated. This is the name to search for.         */
/*                                                          */
/* Exit conditions:                                         */
/* 1) If the name is found, then the return value is the    */
/*    inode number belonging to this name and *indxp is the */
/*    offset in the directory (in bytes) of this entry.     */
/* 2) If the name is not found, the return value is 0 and   */
/*    *indxp is the offset in the directory (in bytes) of   */
/*    the first free slot.                                  */
/*                                                          */
/************************************************************/

static SHORT dir_search(ip, name, indxp)

INODE   *ip;
char    *name;
LONG    *indxp;

{
    SHORT   ino       = 0;
    LONG    offset    = 0L;
    LONG    index     = INO_SIZE(ip);
    int     free_slot = FALSE;
    BLKNO   blkno;
    INT     boff, ct;
    BUFFER  *bufp;
    char    *cp, *limit;

    while ( offset < INO_SIZE(ip) )
    {
        blkno = ino_map(ip, offset, &boff, &ct, (BLKNO *)0, FALSE);
        bufp  = buf_read(INO_DEV(ip), blkno);

        if ( offset + BLOCK_SIZE < INO_SIZE(ip) )
            limit = dbuf(bufp) + BLOCK_SIZE;
        else
            limit = dbuf(bufp) + (int) (INO_SIZE(ip) - offset);

        for (cp = dbuf(bufp); cp < limit; cp += DIR_SIZE, offset += DIR_SIZE)
        {
            if ( *((SHORT *)cp) == 0 )          /* free slot */
            {
                if ( free_slot == FALSE )
                {
                    index     = offset;
                    free_slot = TRUE;
                }

                continue;
            }

            if ( MEMCMP(cp + 2, name, 14) == 0 )    /* found it     */
            {
                ino = *((SHORT *)cp);
                break;          /* leave for loop   */
            }
        }

        buf_release(bufp);

        if ( ino )
            break;              /* leave while loop */
    }

    *indxp = ( ino ) ? offset : index;

    return ino;
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: dir_rm                                         */
/*                                                          */
/* Entry conditions:                                        */
/* 1) ip points to the locked in-core inode of a directory. */
/* 2) offset is the byte offset of a non-deleted entry in   */
/*    this directory (i.e. offset % DIR_SIZE == 0).         */
/* 3) is_dir == TRUE means the entry to be deleted is a     */
/*    directory.                                            */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The entry at position offset is marked deleted.       */
/* 2) The return value is a pointer to the inode whose      */
/*    number was previously in this entry.                  */
/* 3) The inode pointed to by ip has been released.         */
/*                                                          */
/************************************************************/

static INODE *dir_rm(ip, offset, is_dir)

INODE   *ip;
LONG    offset;
int     is_dir;

{
    INT     boff, ct;
    BLKNO   blkno = ino_map(ip, offset, &boff, &ct, (BLKNO *)0, FALSE);
    BUFFER  *bufp = buf_read(INO_DEV(ip), blkno);
    char    *cp   = dbuf(bufp) + boff;
    INODE   *iip  = ino_get(INO_DEV(ip), *((SHORT *)cp));

    *((SHORT *)cp) = 0;     /* mark entry as deleted    */

    buf_write(bufp);

    if ( is_dir )
    {
        INO_UNLINK(ip);     /* because of ".."  */
        INO_UNLINK(iip);    /* because of "."   */
    }

    INO_SET_MTIME(ip, TIME());
    INO_IS_DIRTY(ip);

    ino_put(ip);

    return iip;
}

/*--------------------------------------------------------------*/ 
 
/****************************************************************/
/*                                                              */
/* FUNCTION: dir_chk                                            */
/*                                                              */
/* Entry conditions:                                            */
/* 1) dev is a valid device number.                             */
/* 2) ino is a valid inode number for this device.              */
/*                                                              */
/* Exit conditions:                                             */
/* 1) If the corresponding inode does not belong to a direc-    */
/*    tory, then *is_dir == FALSE and the return value is 0.    */
/* 2) Otherwise *is_dir == TRUE and the return value is:        */
/*      0  if the only entries in the directory are . and ..    */
/*      -1 otherwise.                                           */
/*                                                              */
/****************************************************************/

static int dir_chk(dev, ino, is_dir)

DEV     dev;
SHORT   ino;
int     *is_dir;

{
    INODE   *ip = ino_get(dev, ino);
    int     is_empty = TRUE;
    LONG    offset;
    BLKNO   blkno;
    INT     dummy1, dummy2;
    BUFFER  *bufp;
    char    *cp, *limit;
    
    if ( (INO_MODE(ip) & IFMT) != IFDIR )
    {
        *is_dir = FALSE;
        ino_put(ip);
        return 0;
    }

    *is_dir = TRUE;
    offset  = 0L;

    while ( is_empty && offset < INO_SIZE(ip) )
    {
        blkno = ino_map(ip, offset, &dummy1, &dummy2, (BLKNO *)0, FALSE);
        bufp  = buf_read(INO_DEV(ip), blkno);

        if ( offset + BLOCK_SIZE < INO_SIZE(ip) )
            limit = dbuf(bufp) + BLOCK_SIZE;
        else
            limit = dbuf(bufp) + (int) (INO_SIZE(ip) - offset);

        for ( cp = dbuf(bufp); cp < limit;
                                    cp += DIR_SIZE, offset += DIR_SIZE )
        {
            if ( *((SHORT *)cp) == 0 )              /* free slot */
                continue;

            if ( MEMCMP(cp + 2, dot, 14) == 0 )     /* entry .  */
                continue;

            if ( MEMCMP(cp + 2, dotdot, 14) == 0 )  /* entry .. */
                continue;

            is_empty = FALSE;
            break;
        }

        buf_release(bufp);
    }

    ino_put(ip);

    return ( is_empty ) ? 0 : -1;
}

/*--------------------------------------------------------------*/ 

/************************************************************/
/*                                                          */
/* FUNCTION: dir_create                                     */
/*                                                          */
/* Entry conditions:                                        */
/* 1) ip points to the locked in-core inode of a directory. */
/* 2) offset is the byte offset of a free slot in           */
/*    this directory (i.e. offset % DIR_SIZE == 0).         */
/* 3) name points to a string of exactly 14 characters,     */
/*    (not 0-terminated) -- the name of the new entry.      */
/* 4) uid is a valid user id, gid a valid group id.         */
/*                                                          */
/* Exit conditions:                                         */
/* 1) errno == ENOSPC or else a new disk inode has been     */
/*    assigned and its number entered at position offset.   */
/*    name has also been entered there.                     */
/* 2) The return value is a pointer to the new inode.       */
/* 3) The parent inode has been updated and released.       */
/* 4) The new inode has a link count of 1.                  */
/*                                                          */
/************************************************************/

static INODE *dir_create(ip, offset, name, uid, gid)

INODE   *ip;
LONG    offset;
char    *name;
SHORT   uid;
SHORT   gid;

{
    INT         boff, ct;
    BLKNO       blkno;
    INODE       *iip;
    DIR_ENTRY   entry;
    long        now; 

    if ( INO_SIZE(ip) >= 10L * (LONG) BLOCK_SIZE )
    {
        ino_put(ip);
        IERROR(ENOSPC)
    }

    iip  = sup_ialloc(INO_DEV(ip));

    if ( iip == NIL_INODE )
    {                   /* no more inodes available */
        ino_put(ip);
        IERROR(ENOSPC)
    }

    entry.ino = INO_INO(iip);
    (void) MEMCPY(entry.name, name, 14);

    blkno = ino_map(ip, offset, &boff, &ct, (BLKNO *)0, TRUE); 

    if ( blkno == (BLKNO)0 )
    {
        ino_put(ip);
        ino_put(iip);
        return NIL_INODE;           /* out of space on file system  */
    }

    buf_copyout(INO_DEV(ip), blkno, (char *) &entry, boff, DIR_SIZE, TRUE);

    if ( INO_SIZE(ip) <= offset )
        INO_SET_SIZE(ip, offset + DIR_SIZE);

    now = TIME();

    INO_SET_MTIME(ip, now);
    INO_IS_DIRTY(ip);

    ino_put(ip);

    INO_LINK(iip);
    INO_SET_OWNER(iip, uid, gid);
    INO_SET_ATIME(iip, now);
    INO_SET_MTIME(iip, now);
    INO_SET_CTIME(iip, now);

    return iip;
}

/*--------------------------------------------------------------*/ 

/************************************************************/
/*                                                          */
/* FUNCTION: new_node                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) ip points to the locked in-core inode of a directory. */
/* 2) offset is the byte offset of a free slot in           */
/*    this directory (i.e. offset % DIR_SIZE == 0).         */
/* 3) name points to a string of exactly 14 characters,     */
/*    (not 0-terminated) -- the name of the new entry.      */
/* 4) perms is a valid mode field for an inode.             */
/* 5) dev is a valid device number if perms defines a       */
/*    character special or block special device.            */
/* 6) uid is a valid user id, gid a valid group id.         */
/*                                                          */
/* Exit conditions:                                         */
/* 1) errno == ENOSPC or else a new disk inode has been     */
/*    assigned and its number entered at position offset.   */
/*    name has also been entered there.                     */
/* 2) The parent inode has been updated and released.       */
/* 3) The new inode has been initialized and released.      */
/* 4) The return value is 0 if errno == 0, otherwise -1.    */ 
/*                                                          */
/************************************************************/

static int new_node(ip, offset, name, perms, dev, uid, gid)

INODE   *ip;
LONG    offset;
char    *name;
SHORT   perms;
DEV     dev;
SHORT   uid;
SHORT   gid;

{
    INT         boff, ct;
    BLKNO       blkno;
    INODE       *iip;
    DIR_ENTRY   entry;
    long        now;

    if ( INO_SIZE(ip) >= 10L * (LONG) BLOCK_SIZE )
    {
        ino_put(ip);
        ERROR(ENOSPC)
    }

    iip  = sup_ialloc(INO_DEV(ip));

    if ( iip == NIL_INODE )
    {                   /* no more inodes available */
        ino_put(ip);
        ERROR(ENOSPC)
    }

    entry.ino = INO_INO(iip);
    (void) MEMCPY(entry.name, name, 14);

    blkno = ino_map(ip, offset, &boff, &ct, (BLKNO *)0, TRUE); 

    if ( blkno == (BLKNO)0 )
    {
        ino_put(ip);
        ino_put(iip);
        return -1;                  /* out of space on file system  */ 
    }

    buf_copyout(INO_DEV(ip), blkno, (char *) &entry, boff, DIR_SIZE, TRUE);

    if ( INO_SIZE(ip) <= offset )
        INO_SET_SIZE(ip, offset + DIR_SIZE);

    if ( (perms & IFCHR) == IFCHR || (perms & IFBLK) == IFBLK )
        INO_SET_SIZE(iip, (LONG) dev);

    if ( (perms & IFMT) == IFDIR )
    {
        new_link(iip, 0L, INO_INO(iip), dot);
        new_link(iip, (LONG)DIR_SIZE, INO_INO(ip), dotdot);
        INO_LINK(iip);      /* because of "."   */
        INO_LINK(iip);      /* because of ".."  */
        INO_LINK(ip);
    }
    else
        INO_LINK(iip);
 
    now = TIME();

    INO_SET_MODE(iip, perms);
    INO_SET_OWNER(iip, uid, gid);
    INO_SET_ATIME(iip, now);
    INO_SET_MTIME(iip, now);
    INO_SET_CTIME(iip, now);
    INO_SET_MTIME(ip,  now);
    INO_SET_CTIME(ip,  now);
    INO_IS_DIRTY(iip);
    INO_IS_DIRTY(ip);
 
    ino_put(ip); 
    ino_put(iip);

    return 0;
}

/*--------------------------------------------------------------*/ 

/************************************************************/
/*                                                          */
/* FUNCTION: new_link                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) ip points to the locked in-core inode of a directory. */
/* 2) offset is the byte offset of a free slot in           */
/*    this directory (i.e. offset % DIR_SIZE == 0).         */
/* 3) name points to a string of exactly 14 characters,     */
/*    (not 0-terminated) -- the name of the new entry.      */
/* 4) ino is the number of a valid inode on the same device */
/*    as ip to which a new entry is to be linked.           */
/* 5) dev is the device number of this old inode.           */
/*                                                          */
/* Exit conditions:                                         */
/* 1) errno == ENOSPC or else a new entry has been          */
/*    assigned and ino entered at position offset.          */
/*    name has also been entered there.                     */
/* 2) The parent inode has been updated but not unlocked.   */
/*                                                          */
/************************************************************/

static void new_link(ip, offset, ino, name)

INODE   *ip;
LONG    offset;
SHORT   ino;
char    *name;

{
    INT         boff, ct;
    BLKNO       blkno;
    DIR_ENTRY   entry;

    if ( INO_SIZE(ip) >= 10L * (LONG) BLOCK_SIZE )
    {
        ino_put(ip);
        errno = ENOSPC;
        return;
    }

    entry.ino = ino;
    (void) MEMCPY(entry.name, name, 14);

    blkno = ino_map(ip, offset, &boff, &ct, (BLKNO *)0, TRUE);

    if ( blkno == (BLKNO)0 )
        return;             /* out of space on file system  */ 

    buf_copyout(INO_DEV(ip), blkno, (char *) &entry, boff, DIR_SIZE, TRUE);

    if ( INO_SIZE(ip) <= offset )
        INO_SET_SIZE(ip, offset + DIR_SIZE);

    INO_SET_MTIME(ip, TIME());
    INO_IS_DIRTY(ip);
}

/*--------------------------------------------------------------*/ 

/************************************************************/
/*                                                          */
/* FUNCTION: dir_descend                                    */
/*                                                          */
/* Entry conditions:                                        */
/* 1) pdp points to a structure defining a process (root,   */
/*    cdir, uid, gid).                                      */
/* 2) path points to a valid path string to be converted    */
/*    into a valid inode.                                   */
/*                                                          */
/* Exit conditions:                                         */
/* 1) On exit EITHER errno == 0 and the return value is a   */
/*    pointer to a locked in-core inode belonging to the    */
/*    next to last component in path (parent) OR errno != 0.*/
/* 2) If the last component of path was found, then *inop   */
/*    is the inode number of this last component and *indxp */
/*    is the byte offset of the corresponding entry in the  */
/*    parent directory.                                     */
/* 3) If the last component is not found, *inop == 0 and    */ 
/*    *indxp is the byte offset of the first free slot in   */
/*    in the parent directory.                              */ 
/*                                                          */
/************************************************************/

static INODE *dir_descend(pdp, path, inop, indxp)

PDATA   *pdp;
char    *path;
SHORT   *inop;
LONG    *indxp;

{
    INODE   *ip;
    INODE   *iip;
    char    *p = path;
    char    *q;
    char    name[14];
    int     i;
    LONG    offset;

    if ( *path == '/' )
        ip = ino_get(pdp->rdev, pdp->rino);
    else
        ip = ino_get(pdp->cdev, pdp->cino);
    
    if ( STRCMP(path, "/") == 0 )
    {
        *inop  = INO_INO(ip);
        *indxp = 0L;
        return ip;
    }

    *inop = 0;

    if ( *p == '/' )
        ++p;

    while ( *p )
    {
        /* read next path name component from input */
 
        for (i = 0, q = name; *p && *p != '/' && i < 14; ++i)
            *q++ = *p++;

        while ( i++ < 14 ) 
            *q++ = ' ';

        if ( *p == '/' )
            ++p;

        if ( MEMCMP(name, dotdot, 14) == 0 )
        { 
            if ( INO_INO(ip) == pdp->rino &&  INO_DEV(ip) == pdp->rdev )
            {
                *inop  = INO_INO(ip);
                *indxp = 0L;
                continue;
            }

            if ( INO_INO(ip) == ROOT_INO )      /* crossing mount point */
            {
                iip = sup_mnt_point(ip);
                ino_put(ip);
                ip = ino_get(INO_DEV(iip), INO_INO(iip));
            }
        }
        else if ( INO_MNTPNT(ip) )
        {
            iip = sup_mounted(ip);
            ino_put(ip);
            ip = ino_get(INO_DEV(iip), INO_INO(iip));
        }

        /* verify that ip is directory, access permission OK */

        if ( (INO_MODE(ip) & IFMT) != IFDIR )
            errno = ENOTDIR;
        else if ( (ino_perms(ip, pdp->uid, pdp->gid) & 1) == 0 )
            errno = EACCES;                 /* search not allowed   */

        if ( errno )
        {
            ino_put(ip);
            return NIL_INODE;
        }

        *inop = dir_search(ip, name, &offset);

        if ( *p == '\0' )
            break;              /* that was last component  */

        ino_put(ip);

        if ( *inop == 0 )       /* component not found      */
            IERROR(ENOENT)

        ip = ino_get(INO_DEV(ip), *inop);
    }

    *indxp = offset;

    return ip;
}

/*--------------------------------------------------------------*/ 

static void last_component(path, name)

char    *path;
char    *name;

{
    int     i;
    char    *p, *q;

    for ( p = path + STRLEN(path); p > path; --p ) 
        if ( *(p - 1) == '/' ) 
            break; 
 
    for ( q = name, i = 0; *p && i < 14; ++p, ++i )
        *q++ = *p;

    while ( i++ < 14 )
        *q++ = ' ';
}

