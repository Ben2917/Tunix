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
* This module manages the 'FRAME' data structure for the set
* of page frames, that is the physical memory pages in the
* system.
*
* Page frames can be identified in one of two ways:
* i)  By their frame number (i.e. physical address/PAGE_SIZE).
* ii) By the device number of the swap device and block number
*     on that device in which a valid copy of the frame is to
*     be found, if there is such a copy.
*
*
* Note: unlike the situation with the buffer cache and
*       in-core inodes we do not here require that all
*       page frame descriptors be in some hash list. A
*       descriptor is only on a hash list, if the cor-
*       responding page is swapped, in which case the
*       dev and blkno fields are valid.
*
*       In addition the following axiom must always hold:
*       a desciptor is on the free list exactly when its
*       refcnt field is zero.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/thread.h"
#include    "../include/kcall.h"
#include    "../include/frame.h"
#include    "../include/macros.h"

typedef struct frm
{
    SHORT           intransit;  /* being swapped in or out              */
    SHORT           refcnt;     /* no. of PTE's referencing this frame  */
    DEV             dev;        /* no. of device containing a copy      */
    BLKNO           blkno;      /* no. of block     "       "  "        */
    struct frm      *prev;      /* pointers for free list               */
    struct frm      *next;      /*    "      "   "    "                 */
    struct frm      *pred;      /*    "      "  hash  "                 */
    struct frm      *succ;      /*    "      "   "    "                 */

} FRAME;

#define NIL_FRAME   (FRAME *)0

#define NR_HASH     32
#define HASH_MASK   NR_HASH - 1
#define USER_MEM    256         /* the first page of user memory        */

static int  frame_cnt;          /* tells how many page frames are free  */

/*------------------- Local Data Declarations --------------*/

static FRAME    frames[NR_PAGES];
static FRAME    buckets[NR_HASH];
static FRAME    free_list;

/*------------------- Local Function Declarations ----------*/

static FRAME    *hash_search();
static BLKNO    hash_fkt();

#define FRAME_NR(pp)    ((INT) (pp - frames) + USER_MEM)
#define PFDP(frm_nr)    (frames + frm_nr - USER_MEM)

/*------------------- Public Functions ---------------------*/

void frm_init()

{
    int     i;
    FRAME   *pp = frames;
    FRAME   *fp = &free_list;

    fp->next = fp->prev = fp;

    for ( i = 0; i < NR_HASH; ++i )
        buckets[i].succ = NIL_FRAME;

    for ( i = 0; i < NR_PAGES; ++i, ++pp )
    {
        pp->intransit = FALSE;
        pp->dev       = (DEV)0;
        pp->blkno     = (BLKNO)0;

        pp->next       = fp->next;
        pp->prev       = fp;
        fp->next->prev = pp; 
        fp->next       = pp;

        pp->succ = pp->pred = NIL_FRAME;
    }

    frame_cnt = NR_PAGES;
}

/*----------------------------------------------------------*/

int frm_frame_cnt()

{
    return frame_cnt;
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: frm_alloc                                      */
/*                                                          */
/* Entry conditions:                                        */
/* 1) dev is a legal device number.                         */
/* 2) blkno is a legal block number for this device.        */
/* 3) fnd points to an integer variable that will say       */
/*    whether the frame still had valid contents.           */
/*                                                          */
/* Exit conditions:                                         */
/* 1) If a free frame could be found, the return value is   */
/*    the number of this frame. Otherwise the return value  */
/*    is 0, a value that is otherwise invalid.              */
/* 2) The corresponding frame entry has been intialized     */
/*    for this frame.                                       */
/* 3) If a frame for the given device and block already     */
/*    exists, then the frame number for it is returned.     */
/*                                                          */
/************************************************************/

INT frm_alloc(dev, blkno, fnd)

DEV     dev;
BLKNO   blkno;
int     *fnd;

{
    FRAME   *pp, *hp;

    for (;;)
    {
        if ( (pp = hash_search(dev, blkno)) != NIL_FRAME )
        {
            if ( pp->intransit )
            {
                sleep((EVENT)&pp->intransit);
                continue;
            }

            if ( pp->refcnt == 0 )  /* it's on free list; take it off   */
            {
                pp->next->prev = pp->prev;
                pp->prev->next = pp->next;

                --frame_cnt;
            }

            ++pp->refcnt;

            *fnd = TRUE;

            return FRAME_NR(pp); 
        }

        /* frame not in cache   */

        *fnd = FALSE; 
 
        if ( frame_cnt == 0 )           /* out of frames    */
            return 0;

        /* remove a new frame from free list */
 
        pp = free_list.next;

        pp->next->prev = pp->prev;
        pp->prev->next = pp->next;

        pp->refcnt = 1; 
        --frame_cnt;

        /* reset device and block number information */

        pp->dev   = dev;
        pp->blkno = blkno;
        
        /* remove frame from old hash queue, place on new one */

        hp = &buckets[hash_fkt(dev, blkno)];

        if ( pp->succ != NIL_FRAME )
            pp->succ->pred = pp->pred;
        if ( pp->pred != NIL_FRAME )
            pp->pred->succ = pp->succ;

        if ( hp->succ != NIL_FRAME )
            hp->succ->pred = pp;
        pp->succ = hp->succ;
        pp->pred = hp;
        hp->succ = pp;

        return FRAME_NR(pp);
    }
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: frm_any                                        */
/*                                                          */
/* Entry condtions:                                         */
/*    None                                                  */
/* Exit conditions:                                         */
/*    If any free frame could be found, the return value is */
/*    the number of this frame. Otherwise the return value  */
/*    is 0, a value that is otherwise invalid.              */
/*                                                          */
/************************************************************/

INT frm_any()

{
    FRAME   *pp;

    if ( frame_cnt == 0 )
        return 0;

    /* remove a new frame from free list */
 
    pp = free_list.next;

    pp->next->prev = pp->prev;
    pp->prev->next = pp->next;

    --frame_cnt;


    /* remove from old hash queue, if on one    */

    if ( pp->succ != NIL_FRAME )
        pp->succ->pred = pp->pred;
    if ( pp->pred != NIL_FRAME )
        pp->pred->succ = pp->succ;

    pp->pred = pp->succ = NIL_FRAME;

    pp->refcnt = 1;
    pp->dev    = (DEV)0;
    pp->blkno  = (BLKNO)0;

    return FRAME_NR(pp);
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: frm_realloc                                    */
/*                                                          */
/* Entry condtions:                                         */
/*    frm_nr is the number of a frame returned by frm_alloc */
/*    or frm_any.                                           */
/*                                                          */
/* Exit conditions:                                         */
/*    The binding of the frame to a block on the swap       */
/*    device (if any) is severed and the frame removed from */
/*    the corresponding hash list.                          */  
/*                                                          */
/************************************************************/

void frm_realloc(frm_nr)

INT     frm_nr;

{
    FRAME   *pp = PFDP(frm_nr);

    /* remove from old hash queue, if on one    */

    if ( pp->succ != NIL_FRAME )
        pp->succ->pred = pp->pred;
    if ( pp->pred != NIL_FRAME )
        pp->pred->succ = pp->succ;

    pp->pred = pp->succ = NIL_FRAME;

    pp->dev    = (DEV)0;
    pp->blkno  = (BLKNO)0;
}

/*----------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: frm_free                                       */
/*                                                          */
/* Entry conditions:                                        */
/*    frm_nr is the number of a frame returned by frm_alloc */
/*    or frm_any.                                           */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The refcnt of the corresponding frame is decremented. */
/* 2) If afterward refcnt == 0, then the frame is returned  */
/*    to the free list.                                     */
/*                                                          */
/************************************************************/


void frm_free(frm_nr)

INT     frm_nr;

{
    FRAME   *pp = PFDP(frm_nr);

    --pp->refcnt;

    if ( pp->refcnt == 0 )
    {
        /* put frame on free list */ 

        pp->prev = free_list.prev;
        pp->next = &free_list;

        free_list.prev->next = pp;
        free_list.prev       = pp;

        ++frame_cnt;
    }
}

/*----------------------------------------------------------*/

SHORT frm_refcnt(frm_nr)

INT frm_nr;

{
    return PFDP(frm_nr)->refcnt;
}

/*----------------------------------------------------------*/

void frm_incref(frm_nr)

INT frm_nr;

{
    ++PFDP(frm_nr)->refcnt;
}

/*----------------------------------------------------------*/

void frm_lock(frm_nr)

INT frm_nr;

{
    PFDP(frm_nr)->intransit = TRUE;
}

/*----------------------------------------------------------*/

void frm_unlock(frm_nr)

INT     frm_nr;

{
    FRAME   *pp = PFDP(frm_nr);

    pp->intransit = FALSE;

    wakeup((EVENT) &pp->intransit);
}

/*---------------- Local Functions -----------------------------*/ 

/************************************************************/
/*                                                          */
/* FUNCTION: hash_search                                    */
/*                                                          */
/* hash_search searches the appropriate hash list for a     */
/* frame belonging to device dev and block blkno.           */
/* If such a frame is found, a pointer to it is returned;   */
/* otherwise the null pointer is returned.                  */
/*                                                          */
/************************************************************/

static FRAME *hash_search(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    FRAME   *pp = &buckets[hash_fkt(dev, blkno)];

    for ( pp = pp->succ; pp != NIL_FRAME; pp = pp->succ )
        if ( pp->dev == dev && pp->blkno == blkno )
            break;

    return pp;
}

/*---------------------------------------------------------*/

/*lint  -e715   */

static BLKNO hash_fkt(dev, blkno)

DEV     dev;
BLKNO   blkno;

{
    return (blkno & HASH_MASK);
}

/*lint  +e715   */

/*======================== Test functions ====================*/

typedef struct
{
    SHORT   free;
    SHORT   refcnt;
    SHORT   usage;
    DEV     dev;
    BLKNO   blkno;

} FRAME_DATA;

static swapped[NR_PAGES];

void frm_tell(frmp)

FRAME_DATA *frmp;

{
    INT         i;
    FRAME_DATA  *fp;
    FRAME       *pp;
    extern int  printf();

    for ( i = 0, fp = frmp; i < NR_PAGES; ++i, ++fp)
    {
        fp->refcnt = frames[i].refcnt;
        fp->dev    = frames[i].dev;
        fp->blkno  = frames[i].blkno;
        fp->usage  = 0;
        fp->free   = FALSE;

        swapped[i] = FALSE;
    }

    for ( pp = free_list.next; pp != &free_list; pp = pp->next )
        frmp[pp - frames].free = TRUE;

    for ( i = 0; i < NR_HASH; ++i )
        for ( pp = buckets[i].succ; pp != NIL_FRAME; pp = pp->succ )
        {
            if ( hash_fkt(pp->dev, pp->blkno) != i )
                panic("frame in wrong hash list");

            if ( swapped[pp - frames] )
                panic("frame on more than one hash list");
            else
                swapped[pp - frames] = TRUE;
        }
}
        
