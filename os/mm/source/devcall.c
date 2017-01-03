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
* This module manages the communication with the device
* driver(s).
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/page.h"
#include    "../include/ipc.h"
#include    "../include/comm.h"
#include    "../include/kcall.h"
#include    "../include/devcall.h"
#include    "../include/fcodes.h"
#include    "../include/kmsg.h"
#include    "../include/pid.h"
#include    "../include/chan.h"
#include    "../include/thread.h"
#include    "../include/macros.h"

extern int      errno;

static MSG      msg;              

typedef struct
{
    INT     frm;
    INT     size;
    int     errno;

} BUF;

#define NR_BUF  64

static BUF      buffers[NR_BUF];
static INT      free_buf;

static BUF      *get_buf();
static void     release_buf();
static void     talk();

/*------------------ Definitions of public functions ------------------*/

void dev_init()

{
    int   i;

    for ( i = 0; i < NR_BUF; ++i )
        buffers[i].size = 0;
    
    free_buf  = NR_BUF;
} 

/*---------------------------------------------------------*/

int dev_open(dev)

DEV     dev;

{
    BUF     *bf  = get_buf(); 
    int     err;
    INT     chan = chan_fetch((char *)bf, (char *)&err);

    msg.cmd  = OPEN; 
    msg.arg1 = (ARGTYPE) dev;
    msg.chan = chan;

    talk();
    chan_sleep(chan);

    release_buf(bf);

    errno = err;
    return ( err ) ? -1 : 0;
}

/*---------------------------------------------------------*/

void dev_close(dev)

DEV     dev;

{
    msg.cmd  = CLOSE; 
    msg.arg1 = (ARGTYPE) dev;

    talk();
}

/*---------------------------------------------------------*/

int dev_read(dev, offset, frm, size)

DEV     dev;
LONG    offset;
INT     frm;
INT     size;

{
    BUF     *bf  = get_buf(); 
    int     err;
    INT     chan = chan_fetch((char *)bf, (char *)&err);

    bf->size = size;
    bf->frm  = frm;

    msg.cmd  = READ;
    msg.arg1 = (ARGTYPE) dev;
    msg.arg2 = (ARGTYPE) offset;  
    msg.arg3 = (ARGTYPE) size;
    msg.chan = chan;

    talk();
    chan_sleep(chan); 

    release_buf(bf);

    errno = err;
    return ( err ) ? -1 : 0;
}

/*---------------------------------------------------------*/

int dev_write(dev, offset, frm, size)

DEV     dev;
LONG    offset;
INT     frm;
INT     size;

{
    BUF     *bf  = get_buf(); 
    int     err;
    INT     chan = chan_fetch((char *)bf, (char *)&err);

    kmem_cpout(frm, chan);

    msg.cmd  = WRITE;
    msg.arg1 = (ARGTYPE) dev;
    msg.arg2 = (ARGTYPE) offset; 
    msg.arg3 = (ARGTYPE) size;
    msg.chan = chan;

    talk();
    chan_sleep(chan); 

    release_buf(bf);

    errno = err;
    return ( err ) ? -1 : 0;
}

/*---------------------------------------------------------*/

void dev_op_done(chan, error)

INT     chan;
int     error;

{
    BUF     *bf;
    int     *ep;

    chan_pointers(chan, (char **)&bf, (char **)&ep);

    *ep       = error;
    bf->errno = error;

    chan_wakeup(chan);
}
    
/*---------------------------------------------------------*/

void dev_rd_done(chan, error)

INT     chan;
int     error;

{
    BUF     *bf; 
    int     *ep;

    chan_pointers(chan, (char **)&bf, (char **)&ep);
 
    *ep       = error;
    bf->errno = error;

    if ( error == 0 )
        kmem_cpin(bf->frm, chan);

    chan_wakeup(chan);
}

/*---------------------------------------------------------*/

void dev_wr_done(chan, error)

INT     chan;
int     error;

{
    BUF     *bf;
    int     *ep;

    chan_pointers(chan, (char **)&bf, (char **)&ep);

    *ep       = error;
    bf->errno = error;

    chan_wakeup(chan);
}

/*---------------------------------------------------------*/ 
 
static BUF *get_buf() 

{
    INT i;

    while ( free_buf == 0 )
        sleep((EVENT) &free_buf);

    for ( i = 0; i < NR_BUF; ++i )
        if ( buffers[i].size == 0 )
        {
            buffers[i].size = 1;
            --free_buf;
            return &buffers[i];
        }
}

/*---------------------------------------------------------*/ 

static void release_buf(bf)

BUF     *bf;

{
    bf->size = 0;

    if ( ++free_buf == 1 )
        wakeup((EVENT) &free_buf);
}

/*---------------------------------------------------------*/ 

static void talk()

{
    send(MAX_PID + major((DEV) msg.arg1), &msg, UNCRITICAL);
}

