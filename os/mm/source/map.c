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
* This module manages the 'map' data structure for the set
* of blocks on the swap device. It has to provide on demand
* blocks of QUANTUM consecutive blocks that are free. And it
* has to be able to return single blocks to the free list. It
* also keeps track of how many pages think they own a block.
*
* Currently it only looks after one single swap device (SWAP_DEV)
* but all functions have an argument 'dev', so that one could
* easily extend the system to support multiple swap devices.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/kcall.h"
#include    "../include/map.h"
#include    "../include/macros.h"

#define NR_BLOCKS   1024
#define NR_UNITS    (NR_BLOCKS/QUANTUM)
#define SWAP_DEV    device(1, 0)

typedef struct
{
    INT     first;
    INT     length;

} GAP;

static SHORT    swap_use[NR_BLOCKS];
static GAP      gaps[NR_BLOCKS/2];
static INT      nr_gaps;
static INT      nr_blocks;

/*----------------------------------------------*/

void map_init()

{
    int i;

    for ( i = 0; i < NR_BLOCKS; ++i )
        swap_use[i] = 0;

    gaps[0].first  = 0;
    gaps[0].length = NR_BLOCKS;
    nr_gaps        = 1; 
    nr_blocks      = NR_BLOCKS;
}

/*----------------------------------------------*/

/************************************************/
/*                                              */
/* FUNCTION: map_alloc                          */
/*                                              */
/* Entry conditions:                            */
/*    dev is the device number of a valid swap  */
/*    device.                                   */
/*                                              */
/* Exit conditions:                             */
/*    The return value is the block number of   */
/*    the first of a group of QUANTUM contigu-  */
/*    blocks on the given swap device.          */
/*                                              */
/*    If there is no such group of free blocks  */
/*    available the kernel function 'panic' is  */
/*    called; a value of 0 is returned. (This   */
/*    isn't really a good solution, because 0   */
/*    is a valid block number on a swap device  */
/*    but then the situation is pretty despe-   */
/*    rate anyway.)                             */
/*                                              */
/************************************************/

BLKNO map_alloc(dev)

DEV     dev;

{
    GAP     *gp;
    INT     i, blkno;

    if ( dev != SWAP_DEV )
        return (BLKNO)0;

    if ( nr_blocks == 0 )
    {
        panic("out of space on swap device");
        return (BLKNO)0;
    }

    for ( i = 0, gp = gaps; i < nr_gaps; ++i, ++gp )
        if ( gp->length >= QUANTUM )
            break;

    if ( i >= nr_gaps )
        return (BLKNO)0;

    blkno = gp->first;

    if ( gp->length > QUANTUM )
    {
        gp->length -= QUANTUM;
        gp->first  += QUANTUM;
    }
    else
    {
        if ( i < nr_gaps - 1 )
            (void) MEMMOVE(gp, gp + 1, (nr_gaps - i - 1) * sizeof(GAP));

        --nr_gaps;
    } 

    nr_blocks -= QUANTUM;

    return (BLKNO) blkno;
}

/*----------------------------------------------*/

/************************************************/
/*                                              */
/* FUNCTION: map_inc                            */
/*                                              */
/* Entry conditions:                            */
/*    dev is the device number of a valid swap  */
/*    device and blkno a valid block number for */
/*    this device.                              */
/*                                              */
/* Exit conditions:                             */
/*    The reference count for the given block   */
/*    is incremented. This indicates that an    */
/*    additional page regards the contents of   */
/*    this block as its property.               */
/*                                              */
/************************************************/

void map_inc(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    if ( dev != SWAP_DEV )
        return;

    ++swap_use[(INT)blkno];
}

/*----------------------------------------------*/

/************************************************/
/*                                              */
/* FUNCTION: map_free                           */
/*                                              */
/* Entry conditions:                            */
/*    dev is the device number of a valid swap  */
/*    device and blkno a valid block number for */
/*    this device.                              */
/*                                              */
/* Exit conditions:                             */
/*    The reference count for the given block   */
/*    is decremented. If it becomes 0, then the */
/*    block is returned to the free list.       */
/*                                              */ 
/************************************************/

void map_free(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    GAP     *gp, *pgp;
    INT     i;
    INT     bno = (INT) blkno;

    if ( dev != SWAP_DEV || swap_use[bno] == 0 )
        return;

    if ( --swap_use[bno] )
        return;             /* this block still in use by others    */

    for ( gp = gaps, i = 0; i < nr_gaps; ++gp, ++i )
        if ( bno < gp->first )
            break;

    if ( i >= nr_gaps )
        gp = (GAP *)0;

    if ( i > 0 )
        pgp = &gaps[i - 1];
    else
        pgp = (GAP *)0;

    if ( gp != (GAP *)0 && bno + 1 == gp->first )
    {
        gp->first = bno;
        ++gp->length;
    }
    else if ( pgp != (GAP *)0 && bno == pgp->first + pgp->length )
        ++pgp->length;
    else
    {
        if ( i < nr_gaps )
            (void) MEMMOVE(gp + 1, gp, (nr_gaps - i) * sizeof(GAP));

        gp->first  = bno;
        gp->length = 1;
        ++nr_gaps;
    }

    if ( gp != (GAP *)0 && pgp != (GAP *)0  &&
                        gp->first == pgp->first + pgp->length )
    {
        pgp->length += gp->length;

        if ( i < nr_gaps - 1 )
            (void) MEMMOVE(gp, gp + 1, (nr_gaps - i - 1) * sizeof(GAP));

        --nr_gaps;
    }

    ++nr_blocks;
}

/*======================= Test functions ========================*/

SHORT   *swap_usevect()

{
    return swap_use;
}
