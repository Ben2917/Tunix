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

#include    "../include/ktypes.h" 
#include    "../include/map.h"
#include    "../include/thread.h"


#define MTOTAL  16000

typedef struct header
{
    struct header   *next;      /* next block on free list  */
    INT             size;       /* size of this block       */

} HEADER;

static HEADER   base[MTOTAL];
static HEADER   *freep;
static INT      free;

/*-------------------------------------------------------*/

void    map_init()

{
    freep       = base;
    freep->size = MTOTAL;
    freep->next = freep;

    free = MTOTAL;
}

/*-------------------------------------------------------*/

char    *map_alloc(nbytes, no_block)

INT     nbytes;
int     no_block;

{
    HEADER  *p;
    HEADER  *prevp = freep;
    INT     nunits = 1 + (nbytes + sizeof(HEADER) - 1) / sizeof(HEADER);

    while ( free < nunits )
    {
        if ( no_block )
            return (char *)0;

        sleep((EVENT) &free);
    }

    for ( p = prevp->next; ; prevp = p, p = p->next )
    {
        if ( p->size >= nunits )            /* big enough   */
        {
            if ( p->size == nunits )        /* exact fit    */
                prevp->next = p->next;
            else
            {
                p->size -= nunits;
                p       += p->size;
                p->size  = nunits;
            }

            freep = prevp;

            free -= nunits;

            return (char *) (p + 1);
        }

        if ( p == freep )           /* wrapped around free list */
            return (char *)0;
    }
}

/*-------------------------------------------------------*/

void map_free(ap)

char    *ap;

{
    HEADER  *bp, *p;

    bp = (HEADER *)ap - 1;      /* block header of block being released */

    free += bp->size;

    for ( p = freep; !(bp > p && bp < p->next); p = p->next )
        if ( p >= p->next && (bp > p || bp < p->next) )
            break;      /* freed block at start or end of base  */

    if ( bp + bp->size == p->next ) /* join to upper neighbor   */
    {
        bp->size += p->next->size;
        bp->next  = p->next->next;
    }
    else
        bp->next = p->next;

    if ( p + p->size == bp )        /* join to lower neighbor   */
    {
        p->size += bp->size;
        p->next  = bp->next;
    }
    else
        p->next = bp;

    freep = p;

    wakeup((EVENT) &free);
}
