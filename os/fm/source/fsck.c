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
* This is a utility program for checking the consistency of
* Tunix file systems.
***********************************************************
*/

#include    "../include/fcntl.h"
#include    "../include/macros.h"
#include    "../include/buffer.h"
#include    "../include/inode.h"

#define N_INOS          128
#define N_BLKS          180
#define NO_PER_BLOCK    (BLOCK_SIZE/sizeof(BLKNO))
#define MAX_DEPTH       16

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
 
typedef struct
{
    SHORT   free;
    SHORT   refcnt;

} B_ENTRY;

typedef struct
{
    SHORT   free;
    SHORT   visited;
    SHORT   refcnt;
    SHORT   links;

} I_ENTRY;

char    buf1[BLOCK_SIZE];       /*  for the super block             */
char    buf2[BLOCK_SIZE];       /*  for the inodes                  */
char    buf3[BLOCK_SIZE];       /*  for the single indirect block   */
char    buf4[BLOCK_SIZE];       /*  for the double indirect block   */
char    buf5[BLOCK_SIZE];       /*  for the triple indirect block   */

char    ibuf[MAX_DEPTH][BLOCK_SIZE];
char    bbuf[MAX_DEPTH][BLOCK_SIZE];
BLKNO   iblk[MAX_DEPTH];

B_ENTRY blocks[512];
I_ENTRY inodes[256];

int fd;

void        analyze();
void        search();
D_INODE     *get_inode();
D_ENTRY     *get_block();

/*----------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
    int     i, j;
    DSUPER  *sp = (DSUPER *) buf1;
    LONG    cnt;
    BLKNO   *bnp;
    D_INODE *ip;
    I_ENTRY *ep;

    fd = open(argv[1], O_RDONLY); 

    for ( i = 0; i < 512; ++i )
    {
        blocks[i].free   = FALSE;
        blocks[i].refcnt = 0;
    }

    for ( i = 0; i < 256; ++i )
    {
        inodes[i].free    = TRUE;
        inodes[i].visited = FALSE;
        inodes[i].refcnt  = 0;
        inodes[i].links   = 0;
    }

    for ( i = 0; i < MAX_DEPTH; ++i )
        iblk[i] = (BLKNO)0;

    (void) lseek(fd, (LONG) BLOCK_SIZE, 0);
    (void) read(fd, buf1, BLOCK_SIZE);

    cnt = sp->blk_free;
    i   = sp->b_indx;

    for (;;)
    {
        bnp = &sp->free_blk[i];

        while ( i-- )
        {
            --cnt;
            blocks[*(--bnp)].free = TRUE;
        }

        if ( cnt < N_BLKS )
            break;

        (void) lseek(fd, ((LONG) BLOCK_SIZE) * (*bnp), 0);
        (void) read(fd, (char *)(sp->free_blk), N_BLKS * sizeof(BLKNO));

        i = N_BLKS;
    }

    if ( cnt )
        printf("Count of free blocks incorrect\n");

    cnt = sp->ninodes;
    ep  = inodes;
  
    (void) lseek(fd, ((LONG) BLOCK_SIZE) * 2, 0);
    
    for ( j = 0; j < (int) sp->iblocks; ++j )
    {
        (void) lseek(fd, ((LONG) BLOCK_SIZE) * (j + 2), 0);
        (void) read(fd, buf2, BLOCK_SIZE);
        ip = (D_INODE *) buf2;

        for ( i = 0; cnt && i < INO_PER_BLK; ++i, ++ip, ++ep, --cnt )
            if ( ip->mode )
            {
                ep->free  = FALSE;
                ep->links = ip->nlink;
                analyze(ip);
            }
    }

    search(1,  0);

    for ( i = sp->fst_data; i < sp->tot_size; ++i )
    {
        if ( blocks[i].free && blocks[i].refcnt )
            printf("Block %d is free but referenced\n", i);

        if ( !(blocks[i].free) && blocks[i].refcnt == 0 )
            printf("Block %d is not free but never referenced\n", i);

        if ( blocks[i].refcnt > 1 )
            printf("Block %d is referenced more than once\n", i);
    } 

    for ( i = 0, cnt = 0; i < sp->ninodes; ++i )
    {
        if ( inodes[i].free )
            ++cnt;

        if ( !inodes[i].free && inodes[i].links == 0 )
            printf("Inode %d is not free but has no links\n", i + 1);

        if ( inodes[i].free && inodes[i].refcnt )
            printf("Inode %d is free but referenced\n", i + 1);

        if ( inodes[i].refcnt != inodes[i].links )
            printf("Inode %d has a wrong link count %d\n", i + 1,
													inodes[i].links);
    }

    for ( i = 0; i < (int) sp->i_indx; ++i )
        if ( !inodes[sp->free_ino[i] - 1].free )
            printf("Inode %d in free list but not free\n", sp->free_ino[i]);

    if ( cnt != sp->ino_free )
        printf("Count of free inodes incorrect\n");

    (void) close(fd);
    exit(0);
}

/*---------------------------------------------------*/

void analyze(ip)

D_INODE *ip;

{
    int     i, j, k;
    BLKNO   *bp, *bp1, *bp2;
    SHORT   type = ip->mode & IFMT;

    if ( type == IFCHR || type == IFBLK )
        return;

    for ( i = 0, bp = ip->direct; i < 10; ++i, ++bp )
        if (  *bp != (BLKNO)0 )
            ++blocks[*bp].refcnt;

    if ( ip->indir1 != (BLKNO)0 )
    {
        ++blocks[ip->indir1].refcnt;

        (void) lseek(fd, ((LONG) BLOCK_SIZE) * (ip->indir1), 0);
        (void) read(fd, buf3, BLOCK_SIZE);
        bp = (BLKNO *) buf3;

        for ( i = 0; i < NO_PER_BLOCK; ++i, ++bp )
            if ( *bp != (BLKNO)0 )
                ++blocks[*bp].refcnt;
    }

    if ( ip->indir2 != (BLKNO)0 )
    {
        ++blocks[ip->indir2].refcnt;

        (void) lseek(fd, ((LONG) BLOCK_SIZE) * (ip->indir2), 0);
        (void) read(fd, buf3, BLOCK_SIZE);
        bp = (BLKNO *) buf3;

        for ( i = 0; i < NO_PER_BLOCK; ++i, ++bp )
            if ( *bp != (BLKNO)0 )
            {
                ++blocks[*bp].refcnt;

                (void) lseek(fd, ((LONG) BLOCK_SIZE) * (*bp), 0);
                (void) read(fd, buf4, BLOCK_SIZE);
                bp1 = (BLKNO *) buf4;
    
                for ( j = 0; j < NO_PER_BLOCK; ++j, ++bp1 )
                    if ( *bp1 != (BLKNO)0 )
                        ++blocks[*bp1].refcnt;
            }
    }

    if ( ip->indir3 != (BLKNO)0 )
    {
        ++blocks[ip->indir3].refcnt;

        (void) lseek(fd, ((LONG) BLOCK_SIZE) * (ip->indir3), 0);
        (void) read(fd, buf3, BLOCK_SIZE);
        bp = (BLKNO *) buf3;

        for ( i = 0; i < NO_PER_BLOCK; ++i, ++bp )
            if ( *bp != (BLKNO)0 )
            {
                ++blocks[*bp].refcnt;

                (void) lseek(fd, ((LONG) BLOCK_SIZE) * (*bp), 0);
                (void) read(fd, buf4, BLOCK_SIZE);
                bp1 = (BLKNO *) buf4;
    
                for ( j = 0; j < NO_PER_BLOCK; ++j, ++bp1 )
                    if ( *bp1 != (BLKNO)0 )
                    {
                        ++blocks[*bp1].refcnt;

                        (void) lseek(fd, ((LONG) BLOCK_SIZE) * (*bp1), 0);
                        (void) read(fd, buf5, BLOCK_SIZE);
                        bp2 = (BLKNO *) buf5;

                        for ( k = 0; k < NO_PER_BLOCK; ++k, ++bp2 )
                            if ( *bp2 != (BLKNO)0 )
                                ++blocks[*bp2].refcnt;
                    }
            }
    }
}

/*-----------------------------------------------------*/

void search(ino, lvl)

SHORT   ino;
SHORT   lvl;

{
    D_INODE *ip = get_inode(ino, lvl);
    D_ENTRY *dp;
    int     i, j;

    if ( lvl >= MAX_DEPTH )
    {
        printf("Directory tree too deep\n");
        return;
    }

    ++inodes[ino - 1].refcnt;

    if ( ip->mode == 0 )
        inodes[ino - 1].free = TRUE;

    if ( inodes[ino - 1].visited )
        return;

    inodes[ino - 1].visited = TRUE;

    if ( (ip->mode & IFMT) != IFDIR )
        return;

    for ( i = 0; i < 10; ++i )
    {
        if ( ip->direct[i] == (BLKNO)0 )
            continue;

        dp = get_block(ip->direct[i], lvl);

        for ( j = 0; j < 64; ++j, ++dp )
            if ( dp->ino )
                search(dp->ino, lvl + 1);
    }
}

/*-----------------------------------------------------*/

D_INODE *get_inode(ino, lvl)

SHORT   ino;
SHORT   lvl;

{
    INT     blkno = 2 + (ino - 1) / INO_PER_BLK;
    INT     index = (ino - 1) % INO_PER_BLK;

    if ( iblk[lvl] != blkno )
    {
        (void) lseek(fd, ((long) BLOCK_SIZE) * blkno, 0);
        (void) read(fd, ibuf[lvl], BLOCK_SIZE);
        iblk[lvl] = blkno;
    }

    return ((D_INODE *) ibuf[lvl]) + index;
}

/*-----------------------------------------------------*/

D_ENTRY *get_block(blkno, lvl)

BLKNO   blkno;
SHORT   lvl;

{
    (void) lseek(fd, ((long) BLOCK_SIZE) * blkno, 0);
    (void) read(fd, bbuf[lvl], BLOCK_SIZE);

    return (D_ENTRY *) bbuf[lvl];
}
