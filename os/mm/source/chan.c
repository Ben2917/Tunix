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
#include    "../include/chan.h"
#include    "../include/thread.h"
#include    "../include/macros.h"

#define NR_CHAN 64

typedef struct
{
    char    *ptr1;
    char    *ptr2;
    int     status;

} CHANNEL;

static CHANNEL  channels[NR_CHAN];
static INT      free_chan;

/*------------------------------------------------------*/

void chan_init()

{
    int i;

    for ( i = 0; i < NR_CHAN; ++i )
        channels[i].status = FALSE;

    free_chan = NR_CHAN;
}

/*------------------------------------------------------*/

INT chan_fetch(p1, p2)

char    *p1;
char    *p2;

{
    INT     i;
    CHANNEL *cp;

    while ( free_chan == 0 )
        sleep((EVENT) &free_chan);

    for ( i = 0, cp = channels; i < NR_CHAN; ++i, ++cp )
        if ( cp->status == FALSE )
        {
            cp->status = TRUE;
            cp->ptr1   = p1;
            cp->ptr2   = p2;
            --free_chan;
            return i;
        }
}

/*------------------------------------------------------*/

void chan_sleep(chan)

INT     chan;

{
    sleep((EVENT) &channels[chan]);
}

/*------------------------------------------------------*/

void chan_pointers(chan, pp1, pp2)

INT     chan;
char    **pp1;
char    **pp2;

{
    *pp1 = channels[chan].ptr1;
    *pp2 = channels[chan].ptr2;
}

/*------------------------------------------------------*/

void chan_wakeup(chan)

INT     chan;

{
    wakeup((EVENT) &channels[chan]);
    channels[chan].status = FALSE;
    ++free_chan;
    if ( free_chan == 1 )
        wakeup((EVENT) &free_chan);
}

/*------------------------------------------------------*/

void chan_release(chan)

INT     chan;

{
    channels[chan].status = FALSE;
    ++free_chan;
    if ( free_chan == 1 )
        wakeup((EVENT) &free_chan);
}

