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
* This module is responsible for the representation of
* abstract regions as a collection of physical page frames.
* It looks after such intrinsically hardware dependent
* objects as page tables, although here page tables are
* still reasonably abstract.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/macros.h"
#include    "../include/page.h"
#include    "../include/swap.h"
#include    "../include/frame.h"
#include    "../include/fmcall.h"
#include    "../include/kcall.h"
#include    "../include/region.h"
#include    "../include/fid.h"
#include    "../include/chan.h"
#include    "../include/memmap.h"

/************************************************************/
/*                                                          */
/* Axioms for page descriptor (PAGE):                       */
/* 1) Only if the valid bit is set is the field 'frame'     */
/*    valid. It then contains the number of the page        */
/*    frame assigned to this page.                          */
/* 2) If the 'type' field is SWAP, then the fields 'dev'    */
/*    and 'blk[0]' are valid and contain the device number  */
/*    and block number on that device containing a valid    */
/*    swapped copy of the page.                             */
/* 3) If the type is DEMAND_FILL, then the fields 'dev' and */
/*    'blk' are valid and contain the device number and     */
/*    block numbers in which the contents of the page are   */
/*    to be found (in an executable binary file).           */
/* 4) If the type is DEMAND_ZERO, then the fields 'dev' and */
/*    'blk' are meaningless.                                */
/* 5) If the type is COPIED, then the fields 'dev' and      */
/*    'blk' are meaningless. A page has this type only from */
/*    the moment it is copied by reg_pfault until it is     */
/*    swapped by the page stealer, at which point it gets   */
/*    the type SWAP. Thus a page is always valid (present)  */
/*    when it has type COPIED.                              */
/*                                                          */
/************************************************************/

typedef struct
{
    INT     frame;              /* page frame assigned to page           */
    struct
    {
        INT type:2;             /* DEMAND_ZERO, DEMAND_FILL, SWAP, COPIED */
        INT valid:1;            /* frame is valid                         */
        INT locked:1;           /* to prevent race conditions             */
        INT copy_on_write:1;    /* a page duplicated by reg_dup           */
        INT dirty:1;            /* the page has been written to           */
        INT age:2;              /* used to find a victim for swapping     */
    } flags;
    DEV     dev;                /* device on which copy exists            */
    BLKNO   blk[FACTOR];        /* nos. of blocks holding copy            */

} PAGE;                         /* page descriptor                        */


#define COPIED      3

/* Some useful macros   */

#define GET_TYPE(pdp)           ((pdp)->flags.type)
#define SET_TYPE(pdp, typ)      (pdp)->flags.type = (typ & 03)
#define VALID(rno, indx, pdp)   (pdp)->flags.valid = TRUE;\
                                pte_set_prsnt(rno, indx)
#define INVALID(rno, indx, pdp) (pdp)->flags.valid = FALSE;\
                                pte_set_notprsnt(rno, indx)
#define IS_VALID(pdp)           ((pdp)->flags.valid)
#define LOCK(pdp)               (pdp)->flags.locked = TRUE
#define UNLOCK(pdp)             (pdp)->flags.locked = FALSE
#define IS_LOCKED(pdp)          ((pdp)->flags.locked)
#define CP_WR(pdp)              (pdp)->flags.copy_on_write = TRUE
#define NO_CP_WR(pdp)           (pdp)->flags.copy_on_write = FALSE
#define IS_CP_WR(pdp)           ((pdp)->flags.copy_on_write)
#define DIRTY(pdp)              (pdp)->flags.dirty = TRUE
#define CLEAN(pdp)              (pdp)->flags.dirty = FALSE
#define IS_DIRTY(pdp)           ((pdp)->flags.dirty)
#define GET_AGE(pdp)            ((pdp)->flags.age)
#define SET_AGE(pdp, new_age)   (pdp)->flags.age = (new_age)
#define INC_AGE(pdp)            ++(pdp)->flags.age

typedef struct
{
    LONG    fid;            /* file descriptor of executable    */
    SHORT   type;           /* see region.h                     */
    SHORT   refcnt;         /* how many processes using it?     */
    INT     size;           /* size in pages                    */
    INT     base;           /* base index in page table         */
    PAGE    *pages;         /* points to PAGE-table for region  */ 

} REGION;

/*--------------------- Local data ------------------------------------*/

static REGION   regions[NR_REGIONS];
static REGION   *last_sucker;

/*--------------------- Declarations of local functions ---------------*/

static void     reg_rmv();
static void     reg_shrink();
static void     load_frame();
static short    age_pages();
static void     page_stealer();

#define NEW(n)          (PAGE *) mem_alloc((n) * sizeof(PAGE))
#define RESIZE(p, n)    (PAGE *) mem_realloc((char *)(p), (n) * sizeof(PAGE))
#define DISPOSE(p)      mem_free((char *)(p))

/*--------------------- Definitions of public functions ---------------*/

void reg_init()

{
    int     i;
    REGION  *rp;

    for ( i = 0, rp = regions; i < NR_REGIONS; ++i, ++ rp ) 
    { 
        rp->type = R_FREE; 
        rp->size = 0; 
    } 
 
    last_sucker = regions; 

    chan_init();
    swap_init();
    frm_init();
    mem_init();
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_alloc                                      */
/*                                                          */
/* Entry conditions:                                        */
/* 1) fid is a valid file descriptor. Here is where the     */
/*    executable binary for this region is to be found.     */
/* 2) type is one of those defined in 'region.h'            */
/*                                                          */
/* Exit conditions:                                         */
/*    If there is room in the region table, then the return */
/*    value is the logical number of the region. If no more */
/*    regions are available, the return value is -1. In the */
/*    former case, the region has been initialized with     */
/*    fid and type.                                         */
/*                                                          */
/************************************************************/

int reg_alloc(fid, type)

LONG    fid;
SHORT   type;

{
    REGION  *rp;

    for ( rp = regions; rp < regions + NR_REGIONS; ++rp )
        if ( rp->type == R_FREE )
            break;

    if ( rp >= regions + NR_REGIONS )
        return -1;

    rp->type   = type;
    rp->fid    = fid;
    rp->refcnt = 0;
    rp->size   = 0;

    return (rp - regions);
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_xalloc                                     */
/*                                                          */
/* Entry conditions:                                        */
/* 1) fid is a valid file descriptor. Here is where the     */
/*    executable binary for this region is to be found.     */
/* 2) type is one of those defined in 'region.h'            */
/*                                                          */
/* Exit conditions:                                         */
/*    reg_xalloc is similar to reg_alloc, except that it    */
/*    looks for an already existing region of the given     */
/*    type and with the given descriptor fid. If it finds   */
/*    such a region, it returns its logical number. Other-  */
/*    wise it looks for a free region and initializes it.   */
/*                                                          */
/*    The return value is -1 if the sought region doesn't   */
/*    exist and there is no more room in the region table.  */
/*                                                          */ 
/************************************************************/

int reg_xalloc(fid, type)

LONG    fid;
SHORT   type;

{
    REGION  *rp;

    for ( rp = regions; rp < regions + NR_REGIONS; ++rp )
        if ( rp->type == type && rp->fid == fid )
            break;

    if ( rp < regions + NR_REGIONS )
        return (rp - regions);

    for ( rp = regions; rp < regions + NR_REGIONS; ++rp )
        if ( rp->type == R_FREE )
            break;

    if ( rp >= regions + NR_REGIONS )
        return -1;

    rp->type   = type;
    rp->fid    = fid;
    rp->refcnt = 0;
    rp->size   = 0;

    return (rp - regions);
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_free                                       */
/*                                                          */
/* Entry conditions:                                        */
/*    rno is the logical number of a region allocated by    */
/*    reg_alloc or reg_xalloc.                              */
/*                                                          */
/* Exit conditions:                                         */
/*    If no process is using the region, then it and all    */
/*    its resources are released.                           */
/*                                                          */
/************************************************************/

void reg_free(rno)

int rno;

{
    REGION  *rp = &regions[rno];

    if ( rp->refcnt )
        return;

    reg_rmv(rno);
    rp->type = R_FREE;
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_attach                                     */
/*                                                          */
/* Entry conditions:                                        */
/*    rno is the logical number of a region previously      */
/*    allocated by reg_alloc or reg_xalloc.                 */
/*                                                          */
/* Exit conditions:                                         */
/*    The reference count of the region is incremented to   */
/*    indicate a (further) process is using the region.     */
/*                                                          */
/************************************************************/

void reg_attach(rno)

int rno;

{
    ++regions[rno].refcnt;
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_detach                                     */
/*                                                          */
/* Entry conditions:                                        */
/*    rno is the logical number of a region previously      */
/*    allocated by reg_alloc or reg_xalloc.                 */
/*                                                          */
/* Exit conditions:                                         */
/*    The reference count of the region is decremented to   */
/*    indicate that some process is no longer using the     */
/*    region.                                               */
/*                                                          */
/************************************************************/

void reg_detach(rno)

int rno;

{
    REGION  *rp = &regions[rno];

    if ( rp->refcnt )
        --rp->refcnt;
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_grow                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) rno is the logical number of the region.              */
/* 2) base is the base index in the page table.             */
/* 3) size is the size in pages to be achieved.             */
/* 4) fst is the first logical block number in the exe-     */
/*    cutable binary file, from which the contents for      */
/*    this region are to be taken.                          */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The return value is the page frame number of the page */
/*    table generated for this region. It can be entered in */
/*    a page directory by the caller.                       */
/* 2) If the region already existed, then it is increased   */
/*    (or decreased) to the new size given by the argument  */
/*    size.                                                 */
/* 3) A list of page descriptors of length size is initia-  */
/*    lized.                                                */
/* 4) If the type is R_TEXT, R_DATA_W or R_DATA_R, then the */
/*    list of physical blocks containing the contents for   */
/*    this region is fetched from fm. The block numbers     */
/*    are entered in the corresponding page descriptors.    */
/* 5) A page table is created and the corresponding page    */
/*    table entries are appropriately initialized.          */
/* 6) The pages are all marked invalid, so that they can be */
/*    filled on demand by reg_vfault.                       */
/*                                                          */
/************************************************************/
 
INT reg_grow(rno, base, size, fst)

int     rno;
INT     base;
INT     size;
BLKNO   fst;

{
    REGION  *rp = &regions[rno];
    INT     old_size = rp->size;
    INT     pt_frm;
    PAGE    *pdp;
    INT     i, j;
    SHORT   ptype, pflag;

    pt_frm = ptable((INT) rno);

    if ( size < old_size )
    {
        reg_shrink(rno, size);
        return pt_frm;
    }

    rp->base = base; 
    rp->size = size;

    if ( old_size == 0 )
        rp->pages = NEW(size);
    else
        rp->pages = RESIZE(rp->pages, size);

    if ( rp->type == R_BSS || rp->type == R_STACK ||
            rp->type == R_SHM_R || rp->type == R_SHM_W )
        ptype = DEMAND_ZERO;
    else
        ptype = DEMAND_FILL; 

    if ( rp->type == R_TEXT || rp->type == R_DATA_R )
        pflag = 0;
    else
        pflag = 1;

    if ( ptype == DEMAND_FILL )
        (void) fm_blocks(rp->fid, fst + FACTOR * old_size,
                                        FACTOR * (size - old_size));

    for ( i = old_size, pdp = rp->pages + old_size; i < size; ++i, ++pdp )
    {
        pte_build((INT) rno, base + i, pflag);
        pdp->dev = F_DEV(rp->fid);

        SET_TYPE(pdp, ptype);
        INVALID((INT) rno, base + i, pdp);
        UNLOCK(pdp);
        NO_CP_WR(pdp);
        CLEAN(pdp);
        SET_AGE(pdp, 0);

        if ( ptype == DEMAND_FILL )
            for ( j = 0; j < FACTOR; ++j )
                pdp->blk[j] = fm_nxtblk();
    }

    return pt_frm;
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_dup                                        */
/*                                                          */
/* Entry conditions:                                        */
/*    old_rno is the logical number of the region to be     */
/*    duplicated.                                           */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The new region is an exact copy of the old region.    */
/* 2) All pages in both regions are marked 'copy_on_write'  */
/*    and the 'write'-bits in their page table entries are  */
/*    turned off. Thus a write to any page of either region */
/*    will cause reg_pfault to duplicate the corresponding  */
/*    page. For read-only pages this is of course unneces-  */
/*    sary, but it does no harm.                            */
/* 3) The reference count of the page frame of any valid    */
/*    page of the old region is incremented. Ditto for the  */
/*    swap use count of any swapped page.                   */
/* 4) *ptablp is the frame number of the page table for the */
/*    duplicate region.                                     */
/* 5) The return value is the logical number of the new     */
/*    region.                                               */
/*                                                          */
/************************************************************/

int reg_dup(old_rno, ptablp)

int     old_rno;
INT     *ptablp;

{
    REGION  *orp = &regions[old_rno];
    REGION  *nrp; 
    PAGE    *opdp, *npdp;
    INT     i;
    int     new_rno;
    SHORT   type = orp->type;

    for ( nrp = regions; nrp < regions + NR_REGIONS; ++nrp )
        if ( nrp->type == R_FREE )
            break;

    if ( nrp >= regions + NR_REGIONS )
        return -1;                            

    new_rno = nrp - regions;

    (void) MEMCPY(nrp, orp, sizeof(REGION));

    npdp    = NEW(orp->size);
    opdp    = orp->pages;
    *ptablp = ptable((INT) new_rno);

    ptabl_dup((INT) old_rno, (INT) new_rno);

    (void) MEMCPY(npdp, opdp, orp->size * sizeof(PAGE));

    nrp->pages = npdp;

    for ( i = 0; i < orp->size; ++i, ++opdp, ++npdp )
    {
        if ( IS_VALID(opdp) )
            frm_incref(opdp->frame);

        if ( GET_TYPE(opdp) == SWAP )
            swap_inc(opdp->dev, opdp->blk[0]);

        if ( type == R_TEXT || type == R_DATA_R || type == R_SHM_R )
            continue;

        CP_WR(opdp);
        CP_WR(npdp);

        pte_notwrite((INT) old_rno, orp->base + i);
        pte_notwrite((INT) new_rno, nrp->base + i);
    }

    return new_rno;
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_vfault                                     */
/*                                                          */
/* Entry conditions:                                        */
/* 1) rno is the region number of the region in which the   */
/*    fault occurred.                                       */
/* 2) indx is the index in the page table of the page       */
/*    involved.                                             */
/*                                                          */
/* Exit conditions:                                         */
/* 1) reg_vfault is called by the fault handler and has     */
/*    the task of finding a free page frame for the page    */
/*    belonging to the region rno and index indx and fil-   */
/*    ling it appropriately.                                */
/* 2) If the page is of type DEMAND_ZERO, the new frame     */
/*    is simply filled with zeros.                          */
/* 3) If the page is of type DEMAND_FILL, it must be filled */
/*    with the contents of the corresponding block in the   */
/*    executable binary.                                    */
/* 4) If the page is swapped (type SWAP), then the copy on  */
/*    the swap device must be fetched and copied into the   */
/*    new frame.                                            */
/* 5) The page is then marked valid and the 'present'-bit   */
/*    in the page table entry is turned on.                 */
/* 6) The return value should normally be 0. It is -1 if    */
/*    the validity fault is genuine, i.e. the address is    */
/*    outside the range of the region. It is also -1 if     */
/*    no page frame could be assigned, something that can't */
/*    happen....!!??                                        */
/*                                                          */
/************************************************************/

int reg_vfault(rno, indx)

INT     rno;
INT     indx;

{
    REGION  *rp = &regions[rno];
    PAGE    *pdp;

    pdp = rp->pages + indx - rp->base;

    load_frame(rno, indx);

    return ( pdp->frame ) ? 0 : -1;
}

/*---------------------------------------------------------------*/

void reg_iodone(frm_nr, chan, res, error)

INT     frm_nr;
INT     chan;
int     res;
int     error;

{
    fm_iodone(frm_nr, chan, res, error);
}

/*---------------------------------------------------------------*/

void reg_blkdone(chan, res, error)

INT chan;
int res;
int error;

{
    fm_blkdone(chan, res, error);
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_dump                                       */
/*                                                          */
/* Entry conditions:                                        */
/* 1) rno is the logical number of the region to be dumped. */
/* 2) pid is the process identifier of the process being    */
/*    dumped. The file manager needs this.                  */
/* 3) fd is the file descriptor of an open file into which  */
/*    the core dump is to be written.                       */
/*                                                          */
/* Exit conditions:                                         */
/* 1) The contents of the entire region are written to the  */
/*    file with descriptor fd.                              */
/* 2) Any pages which are invalid when reg_dump is called   */
/*    are first loaded into memory; this includes pages     */
/*    which have never been in memory and are still in the  */
/*    original executable binary file.                      */
/*                                                          */
/************************************************************/

int reg_dump(rno, pid, fd)

int     rno;
INT     pid;
int     fd;

{
    REGION  *rp = &regions[rno];
    PAGE    *pdp;
    INT     i;

    for ( i = 0, pdp = rp->pages; i < rp->size; ++i, ++pdp )
    {
        if ( !IS_VALID(pdp) )
            load_frame((INT) rno, rp->base + i);

        if ( pdp->frame == 0 )
            return -1;

        if ( fm_dump(pid, fd, pdp->frame) == -1 )
            return -1;
    }
    
    return 0;
}

/*---------------------------------------------------------------*/

void reg_dumpdone(chan, res, error)

INT     chan;
int     res;
int     error;

{
    fm_done(chan, res, error);
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_pfault                                     */
/*                                                          */
/* Entry conditions:                                        */
/* 1) rno is the region number of the region in which the   */
/*    fault occurred.                                       */
/* 2) indx is the index in the page table of the page       */
/*    involved.                                             */
/*                                                          */
/* Exit conditions:                                         */
/* 1) reg_pfault is called by the fault handler and has     */
/*    something to do only if the page was 'copy-on-write'. */
/*    In all other cases it should return -1, because a     */
/*    genuine protection fault has occurred.                */
/* 2) If the page is not valid (present), then reg_pfault   */
/*    returns a value of zero and lets the validity hand-   */
/*    ler take care of the case first. (reg_pfault will be  */
/*    called again immediately.)                            */
/* 3) If some other page is using the same page frame, then */
/*    the reference count must be decremented, a new page   */
/*    frame assigned for this page and the old contents     */
/*    copied into it.                                       */
/* 4) If the page has been swapped, then the copy on the    */
/*    swap device must be released.                         */
/* 5) The 'copy-on-write' flag is turned off and the        */
/*    'write'-bit in the page table entry turned on.        */
/* 6) The return value is 1 to signalized that all is well. */
/*                                                          */
/************************************************************/

int reg_pfault(rno, indx)

INT     rno;
INT     indx;

{
    REGION  *rp = &regions[rno];
    PAGE    *pdp;
    INT     ofrm, nfrm;

    pdp = rp->pages + indx - rp->base;

    if ( !IS_CP_WR(pdp) )
        return -1;

    if ( !IS_VALID(pdp) )
        return 0;                       /* let vfault do its thing first */

    LOCK(pdp); 
 
    if ( frm_frame_cnt() < LO_WATER )
        page_stealer();

    ofrm = pdp->frame;

    if ( frm_refcnt(ofrm) > 1 )
    {
        nfrm = frm_any(); 
 
        if ( nfrm )
            mem_copy(nfrm, ofrm);

        frm_free(ofrm);         /* decrement refcnt */

        pdp->frame = nfrm; 
        pte_set_frame(rno, indx, nfrm);
        VALID(rno, indx, pdp); 
    }

    if ( GET_TYPE(pdp) == SWAP )
        swap_free(pdp->dev, pdp->blk[0]);   /* decrement refcnt         */

    frm_realloc(pdp->frame);        /* break connection to swapped copy */

    SET_TYPE(pdp, COPIED);
    SET_AGE(pdp, 0);
    pte_write(rno, indx); 
    NO_CP_WR(pdp);
    DIRTY(pdp);     /* force use of new block, in case this page reswapped */
    UNLOCK(pdp);

    return ( pdp->frame ) ? 1 : -1;
}

/*----------------- Definitions of local functions --------------------*/


/************************************************************/
/*                                                          */
/* FUNCTION: reg_rmv                                        */
/*                                                          */
/* Entry conditions:                                        */
/*    rno is the logical number of the region to be         */
/*    removed.                                              */
/*                                                          */
/* Exit conditions:                                         */
/* 1) All page descriptors for the region are released.     */
/* 2) The page table for the region is released.            */
/* 3) The reference count of the page frame of any valid    */
/*    page of the region is decremented. Ditto for the      */
/*    swap use count of any swapped page.                   */
/*                                                          */
/************************************************************/

static void reg_rmv(rno)

int     rno;

{
    REGION  *rp = &regions[rno];
    PAGE    *pdp;
    INT     i;

    if ( rp->pages == (PAGE *)0 )
        return;

    for ( i = 0, pdp = rp->pages; i < rp->size; ++i, ++pdp )
    {
        if ( GET_TYPE(pdp) == SWAP )
            swap_free(pdp->dev, pdp->blk[0]);

        if ( IS_VALID(pdp) )
            frm_free(pdp->frame);
    }

    DISPOSE(rp->pages);

    rp->size = 0;
}
/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: reg_shrink                                     */
/*                                                          */
/* Entry conditions:                                        */
/* 1) rno is the logical number of a region.                */
/* 2) size is the new (smaller) size in pages that the      */
/*    region should now have.                               */
/*                                                          */
/* Exit conditions:                                         */
/* 1) This function is called by reg_grow if it determines  */
/*    that an existing region is to shrink. It has the job  */
/*    of releasing the no longer needed pages and adjusting */
/*    the 'size' field.                                     */
/* 2) If any page had a page frame assigned to it, the      */
/*    reference count of the frame must be decremented.     */
/* 3) If there is a copy of the page on the swap device,    */
/*    the swap use count of the copy must be decremented.   */
/*                                                          */
/************************************************************/

static void reg_shrink(rno, size)

int     rno;
INT     size;

{
    REGION  *rp = &regions[rno];
    PAGE    *pdp;
    INT     i;

    for ( i = size, pdp = rp->pages + size; i < rp->size; ++i, ++pdp )
    {
        if ( GET_TYPE(pdp) == SWAP )
            swap_free(pdp->dev, pdp->blk[0]);

        frm_free(pdp->frame);
    }

    rp->pages = RESIZE(rp->pages, size);
    rp->size  = size;
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: load_frame                                     */
/*                                                          */
/* Entry conditions:                                        */
/* 1) rno is the number of the region to which the frame    */
/*    belongs.                                              */
/* 2) indx is the index of the page within the page table.  */
/*                                                          */
/* Exit conditions:                                         */
/* 1) load_frame is called by reg_vfault and also by        */
/*    reg_dump to get a page into memory. It has the task   */
/*    of finding a free page frame for the page desribed by */
/*    rno and indx and filling it appropriately.            */
/* 2) If the page is of type DEMAND_ZERO, the new frame     */
/*    is simply filled with zeros.                          */
/* 3) If the page is of type DEMAND_FILL, it must be filled */
/*    with the contents of the corresponding block in the   */
/*    executable binary.                                    */
/* 4) If the page is swapped (type SWAP), then the copy on  */
/*    the swap device must be fetched and copied into the   */
/*    new frame.                                            */
/* 5) The page is then marked valid and the 'present'-bit   */
/*    in the page table entry is turned on.                 */
/* 6) load_frame does not do its job and pdp->frame == 0,   */
/*    if no page frame could be assigned, something that    */
/*    can't happen....!!??                                  */
/*                                                          */
/************************************************************/

static void load_frame(rno, indx)

INT     rno;
INT     indx;

{  
    REGION  *rp = &regions[rno];
    PAGE    *pdp;
    int     found;

    pdp = rp->pages + indx - rp->base;

    LOCK(pdp);

    if ( frm_frame_cnt() < LO_WATER )
        page_stealer();

    switch ( GET_TYPE(pdp) )
    {
    case DEMAND_ZERO:
        pdp->frame = frm_any();

        if ( pdp->frame == 0 )
        {
            UNLOCK(pdp);
            return;
        }

        mem_zero(pdp->frame);
        break;
    case DEMAND_FILL:
        pdp->frame = frm_alloc(pdp->dev, pdp->blk[0], &found);

        if ( pdp->frame == 0 )
        {
            UNLOCK(pdp);
            return;
        }

        if ( !found )
        {
            frm_lock(pdp->frame);
            (void) fm_fill(pdp->frame, pdp->dev, pdp->blk);
            frm_unlock(pdp->frame);
        }

        break;
    case SWAP:
        pdp->frame = frm_alloc(pdp->dev, pdp->blk[0], &found);

        if ( pdp->frame == 0 )
        {
            UNLOCK(pdp);
            return;
        }

        if ( !found)
        {
            frm_lock(pdp->frame);
            swap_in(pdp->frame, pdp->dev, pdp->blk[0]);
            frm_unlock(pdp->frame);
        }

        break;
    }

    pte_set_frame(rno, indx, pdp->frame); 

    if ( IS_CP_WR(pdp) )
        pte_notwrite(rno, indx);

    VALID(rno, indx, pdp);
    SET_AGE(pdp, 0); 

    CLEAN(pdp);
    UNLOCK(pdp);
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: age_pages                                      */
/*                                                          */
/* This function is called by the page stealer. It has the  */
/* job of going through all pages of all regions and re-    */
/* computing their 'age' on the basis of whether they have  */
/* been accessed since the last page stealing session.      */
/*                                                          */
/* It also marks pages as dirty, if they have been written  */
/* to since the last pass through. It resets the correspon- */
/* ding bits in the page table entry.                       */
/*                                                          */
/************************************************************/

static short age_pages()

{
    REGION  *rp;
    PAGE    *pdp;
    INT     i, j;
    short   age, old = 0;

    for ( i = 0, rp = regions; i < NR_REGIONS; ++i, ++rp )
    {
        if ( rp->type == R_FREE )
            continue;

        for ( j = 0, pdp = rp->pages; j < rp->size; ++j, ++pdp )
        {
            if ( !IS_VALID(pdp) )
                continue;

            if ( !pte_access(i, rp->base + j) )
            {
                if ( (age = (short) GET_AGE(pdp)) < 3 )
                {
                    INC_AGE(pdp);
                    ++age;
                }

                if ( age > old )
                    old = age;
            }
            else
            {
                SET_AGE(pdp, 0);

                if ( pte_dirty(i, rp->base + j) )
                    DIRTY(pdp);
            }
        }
    }

    /* Now reset acces and dirty bits of all PTEs   */

    for ( i = 0, rp = regions; i < NR_REGIONS; ++i, ++rp )
    {
        if ( rp->type == R_FREE )
            continue;

        for ( j = 0, pdp = rp->pages; j < rp->size; ++j, ++pdp )
        {
            if ( !IS_VALID(pdp) )
                continue;

            pte_reset(i, rp->base + j);
        }
    }   

    return old;
}

/*---------------------------------------------------------------*/

/************************************************************/
/*                                                          */
/* FUNCTION: page_stealer                                   */
/*                                                          */
/* This function is called when the number of free page     */
/* frames drops below LO_WATER. It has to find victims to   */
/* be swapped in order to free more frames. It does so until*/
/* the number of free frames reaches the level HI_WATER.    */
/*                                                          */
/* The page stealer frees the frames belonging to all pages */
/* it finds which are older than its current maximum.       */
/*                                                          */
/* If the page is dirty, it has to be swapped to the swap   */
/* device. In any case the page frame is released and the   */
/* page marked invalid. The 'present'-bit in the page table */
/* entry is turned off.                                     */
/*                                                          */
/************************************************************/

static void page_stealer()

{
    REGION  *rp;
    PAGE    *pdp;
    INT     i, n;
    short   old;

    for ( old = age_pages(); old >= 0; --old)
    {
        for ( n = 0, rp = last_sucker + 1; n < NR_REGIONS; ++n, ++rp )          
        {
            if ( rp >= regions + NR_REGIONS )
                rp = regions;

            if ( rp->type == R_FREE )
                continue;

            for ( i = 0, pdp = rp->pages; i < rp->size; ++i, ++pdp )
            {
                if ( IS_LOCKED(pdp) )
                    continue;

                if ( IS_VALID(pdp) && ((short)GET_AGE(pdp) >= old) )
                {
                    if ( IS_DIRTY(pdp) )
                    {
                        if ( GET_TYPE(pdp) == SWAP )
                            swap_free(pdp->dev, pdp->blk[0]);

                        swap_out(pdp->frame, &pdp->dev, pdp->blk);
                        SET_TYPE(pdp, SWAP);
                    }

                    frm_free(pdp->frame); 
                    INVALID(n, rp->base + i, pdp);
                    SET_AGE(pdp, 0);
                }

                if ( frm_frame_cnt() >= HI_WATER )
                {
                    last_sucker = rp;
                    return;
                }
            }
        }
    }
}

/*================== Test functions ===========================*/

typedef struct
{
    SHORT   free;
    SHORT   refcnt;
    SHORT   usage;
    DEV     dev;
    BLKNO   blkno;

} FRAME_DATA;

static SHORT    block_use[1024];

static FRAME_DATA   frames[NR_PAGES];

void reg_test()

{
    INT     i, j;
    REGION  *rp;
    PAGE    *pdp;
    SHORT   *suse, *swap_usevect();
    extern void frm_tell();
    static void report();

    for ( i = 0; i < 1024; ++i )
        block_use[i] = 0;

    frm_tell(frames);

    for ( i = 0, rp = regions; i < NR_REGIONS; ++i, ++rp )
    {
        if ( rp->type == R_FREE )
            continue;

        for ( j = 0, pdp = rp->pages; j < rp->size; ++j, ++pdp )
        {
            switch( GET_TYPE(pdp) )
            {
            case DEMAND_ZERO:
            case DEMAND_FILL: 
            case COPIED:
                if ( IS_VALID(pdp) )
                {
                    ++frames[pdp->frame & 255].usage;
                    if ( pdp->frame != pte_get_frame(i, rp->base + j) )
                        report("Two frame numbers don't agree");
                    if ( !pte_is_prsnt(i, rp->base + j) )
                        report("Page valid but frame not present"); 
                }
                break;
            case SWAP:
                if ( IS_VALID(pdp) )
                {
                    ++frames[pdp->frame & 255].usage;
                    if ( pdp->frame != pte_get_frame(i, rp->base + j) )
                        report("Two frame numbers don't agree");
                    if ( !pte_is_prsnt(i, rp->base + j) )
                        report("Page valid but frame not present"); 

                    if ( pdp->dev != frames[pdp->frame & 255].dev ||
                         pdp->blk[0] != frames[pdp->frame & 255].blkno )
                        report("Page and frame in different blocks");
                }

                ++block_use[pdp->blk[0]];  
                break;
            }
        }
    }

    for ( i = 0; i < NR_PAGES; ++i )
        if ( frames[i].refcnt != frames[i].usage )
            report("Usage and refcnt don't agree for a frame");
    
    suse = swap_usevect();

    for ( i = 0; i < 1024; ++i )
        if ( suse[i] != block_use[i] )
            report("Block use and swap use don't agree");
}


static void report(s)

char *s;

{
    extern int printf();

    (void) printf("mmtest: %s\n", s);
}



