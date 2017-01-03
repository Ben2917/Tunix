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

struct stat
{
    DEV     tx_dev;     /* device containing directory entry    */
    SHORT   tx_ino;     /* inode number                         */
    SHORT   tx_mode;    /* file mode, see below                 */
    SHORT   tx_nlink;   /* number of links                      */
    SHORT   tx_uid;     /* owner uid                            */
    SHORT   tx_gid;     /* owner gid                            */
    DEV     tx_rdev;    /* id of device. Special files only     */
    LONG    tx_size;    /* file size in bytes                   */
    long    tx_atime;   /* time of last access                  */
    long    tx_mtime;   /* time of last data modification       */
    long    tx_ctime;   /* time of last file status change      */
};

/* Masks for mode */

#define IFMT      0xf000      /* type of file         */
#define IFDIR     0x4000      /* directory            */
#define IFCHR     0x2000      /* character special    */
#define IFBLK     0x6000      /* block special        */
#define IFREG     0x8000      /* regular file         */
#define IFIFO     0x1000      /* named pipe           */
#define ISUID     0x0800      /* setuid bit           */
#define ISGID     0x0400      /* setgid bit           */
#define ISVTX     0x0200      /* sticky bit           */


