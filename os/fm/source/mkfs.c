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
* This is a utility program for building Tunix file systems.
************************************************************
*/

#include    "../include/fcntl.h"
#include    "../include/macros.h"
#include    "../include/buffer.h"
#include    "../include/inode.h"

#define N_INOS          128
#define N_BLKS          180
#define NO_PER_BLOCK    (BLOCK_SIZE/sizeof(BLKNO))

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
    SHORT ino;
    char name[14];

} D_ENTRY; 
 


char    buf[BLOCK_SIZE];

main(argc, argv)

int  argc;
char **argv;

{
    int     fd, i, indx, remember;
    SHORT   niblks;
    DSUPER  *sp = (DSUPER *)buf;
    D_INODE *ip = (D_INODE *)buf;
    D_ENTRY *entry;
    BLKNO   *bnp;

    fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    (void) write(fd, buf, BLOCK_SIZE);  /* the boot block   */
    
    niblks       = 1 + 255 / INO_PER_BLK;
    indx         = (509 - (int) niblks) % N_BLKS;
    sp->tot_size = 512;
    sp->ninodes  = 256;
    sp->iblocks  = niblks;
    sp->fst_data = 2 + niblks;
    sp->max_size = ((LONG)NO_PER_BLOCK) * ((LONG)NO_PER_BLOCK) * ((LONG)NO_PER_BLOCK);
    sp->blk_free = sp->tot_size  - sp->fst_data - 1;
    sp->ino_free = sp->ninodes - 1;
    sp->i_indx   = N_INOS;
    sp->b_indx   = indx;

    for ( i = 0; i < N_INOS; ++i )
        sp->free_ino[i] = (SHORT) (N_INOS - i + 1);

    for ( i = 0; i < indx; ++i )
        sp->free_blk[i] = (BLKNO) (indx - i + niblks + 2);

    remember = sp->free_blk[0];

    (void) write(fd, buf, BLOCK_SIZE);  /* the super block  */

    ip->mode  = IFDIR | 0755;
    ip->nlink = (SHORT) 3;  /* "." and ".." and root        */
    ip->uid   = ROOT_UID;
    ip->gid   = ROOT_GID;
    ip->size  = 32;         /* two entries: "." and ".."    */
    ip->atime = 0L;
    ip->mtime = 0L;
    ip->ctime = 0L;

    ip->direct[0] = (BLKNO) (2 + niblks);
    
    for ( i = 1; i < 10; ++i )
        ip->direct[i] = (BLKNO) 0;

    ip->indir1 = (BLKNO) 0;
    ip->indir2 = (BLKNO) 0;
    ip->indir3 = (BLKNO) 0;

    for ( i = 1; i < INO_PER_BLK; ++i )
    {
        (void) MEMCPY(ip + 1, ip, sizeof(D_INODE));
        (++ip)->mode = (SHORT) 0;
    }

    ip = (D_INODE *) buf;

    for ( i = 0; i < (int) niblks; ++i )
    {
        (void) write (fd, buf, BLOCK_SIZE);
        ip->mode = 0;
    }
        
    (void) MEMSET(buf, '\0', 1024);
    entry = (D_ENTRY *) buf;
    entry->ino = (SHORT) 1;
    (void) MEMCPY(entry->name, ".             ", 14);
    ++entry;
    entry->ino = (SHORT) 1;
    (void) MEMCPY(entry->name, "..            ", 14);

    (void) write(fd, buf, BLOCK_SIZE);      /* Block 1; root directory  */

    for ( i = 0, bnp = (BLKNO *)buf; i < N_BLKS; ++i )
        *bnp++ = (N_BLKS + remember - i);

    (void) lseek(fd, ((LONG) remember) * BLOCK_SIZE, 0);
    (void) write(fd, buf, BLOCK_SIZE);      /* 1. list of free blocks   */

    remember += N_BLKS;

    for ( i = 0, bnp = (BLKNO *)buf; i < N_BLKS; ++i )
        *bnp++ = (N_BLKS + remember - i);

    (void) lseek(fd, ((LONG) remember) * BLOCK_SIZE, 0);
    (void) write(fd, buf, BLOCK_SIZE);      /* 2. list if free blocks   */

    (void) close(fd);

    exit(0);
}
