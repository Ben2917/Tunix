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
* This module does the actual swapping of page frames to
* the swap device(s).
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/devcall.h"
#include    "../include/kcall.h"
#include    "../include/map.h"
#include    "../include/swap.h"
#include    "../include/macros.h"

#define SWAP_DEV    device(1, 0)

/*----------------------------------------------------------*/

void swap_init()

{
    dev_init();
    map_init();

    if  ( dev_open(SWAP_DEV) == -1 )
        panic("can't open swap device");
}

/*----------------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: swap_in                                */
/*                                                  */
/* Entry conditions:                                */
/* 1) frm_nr is the number of a valid page frame.   */
/* 2) dev is the number of a valid swap device.     */
/* 3) blkno is the number of a valid block on the   */
/*    given swap device, a block containing a copy  */
/*    of the contents of a page.                    */
/*                                                  */
/* Exit conditions:                                 */
/*    The contents of block number blkno on swap    */
/*    device dev are copied into the page frame     */
/*    number frm_nr.                                */
/*                                                  */
/****************************************************/

void swap_in(frm_nr, dev, blkno)

INT     frm_nr;
DEV     dev;
BLKNO   blkno;

{
    (void) dev_read(dev, ((LONG)PAGE_SIZE) * blkno, frm_nr, PAGE_SIZE);
}

/*----------------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: swap_out                               */
/*                                                  */
/* Entry conditions:                                */
/*    frm_nr is the number of a valid page frame.   */
/*                                                  */
/* Exit conditions:                                 */
/* 1) An appropriate swap device is chosen and a    */
/*    free block on that device found into which    */
/*    the contents of page frame frm_nr can be      */
/*    copied. *devp and *blkp are assigned the      */
/*    corresponding numbers.                        */
/* 2) The page frame frm_nr is transferred to the   */
/*    given block.                                  */
/*                                                  */
/****************************************************/

void swap_out(frm_nr, devp, blkp)

INT     frm_nr;
DEV     *devp;
BLKNO   *blkp;

{
    *devp = SWAP_DEV;
    *blkp = map_alloc(SWAP_DEV);

    (void) dev_write(*devp, ((LONG)PAGE_SIZE) * (*blkp), frm_nr, PAGE_SIZE);
}

/*----------------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: swap_inc                               */
/*                                                  */
/* Entry conditions:                                */
/* 1) dev is the number of a valid swap device.     */
/* 2) blkno is the number of a valid block on the   */
/*    given swap device, a block containing a copy  */
/*    of the contents of a page.                    */
/*                                                  */
/* Exit conditions:                                 */
/*    The reference count of block number blkno on  */
/*    swap device dev is incremented to indicate    */
/*    that an additional page is interested in the  */
/*    copy in that block.                           */
/*                                                  */
/****************************************************/

void swap_inc(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    map_inc(dev, blkno);
}

/*----------------------------------------------------------*/

/****************************************************/
/*                                                  */
/* FUNCTION: swap_free                              */
/*                                                  */
/* Entry conditions:                                */
/* 1) dev is the number of a valid swap device.     */
/* 2) blkno is the number of a valid block on the   */
/*    given swap device, a block containing a copy  */
/*    of the contents of a page.                    */
/*                                                  */
/* Exit conditions:                                 */
/*    The reference count of block number blkno on  */
/*    swap device dev is decremented to indicate    */
/*    that some page is no longer interested in the */
/*    copy in that block.                           */
/*                                                  */
/****************************************************/

void swap_free(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    map_free(dev, blkno);
}



