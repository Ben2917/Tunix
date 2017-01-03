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
* This is a utility program for inspecting the block- and
* inode usage in Tunix file systems.
***********************************************************
*/

#include    "../include/fcntl.h"
#include    "../include/buffer.h"

#define N_INOS          128
#define N_BLKS          180
 
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

char buf[BLOCK_SIZE];

/*----------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
    int     fd, percent;
    DSUPER  *sp = (DSUPER *) buf;

    fd = open(argv[1], O_RDONLY);

    (void) lseek(fd, (LONG) BLOCK_SIZE, 0);
    (void) read(fd, buf, BLOCK_SIZE);
    (void) close(fd);

    percent = 100 - (int) ((100L * sp->blk_free) / sp->tot_size);

    printf("Blocks: %ld free of %ld total    %d%% in use\n",
                        sp->blk_free, sp->tot_size, percent);

    printf("Inodes: %d free of %d total\n", sp->ino_free, sp->ninodes);

    exit(0);
}
