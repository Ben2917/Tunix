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
#include    "../include/devcall.h"
#include    "../include/fcodes.h"
#include    "../include/kmsg.h"
#include    "../include/buffer.h"
#include    "../include/pid.h"
#include    "../include/chan.h"
#include    "../include/thread.h"
#include    "../include/macros.h"

extern int      errno;

static MSG      msg;              

typedef struct
{
    CHAR    *buf;
    INT     size;
    int     errno;
    int     flag;           /* SYNCH or ASYNCH  */

} BUF;

#define SYNCH   0
#define ASYNCH  1

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

    msg.cmd   = OPEN; 
    msg.arg1  = (ARGTYPE) dev;
    msg.chan  = chan;

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

int dev_sread(dev, offset, buf, size)       /* synchronous read */

DEV     dev;
LONG    offset;
CHAR    *buf;
INT     size;

{
    BUF     *bf  = get_buf(); 
    int     err;
    int     res;
    INT     chan = chan_fetch((char *)bf, (char *)&err);

    bf->size = size;
    bf->buf  = buf;
    bf->flag = SYNCH;

    msg.cmd  = READ;
    msg.arg1 = (ARGTYPE) dev;
    msg.arg2 = (ARGTYPE) offset;  
    msg.arg3 = (ARGTYPE) size;
    msg.chan = chan;

    talk();
    chan_sleep(chan); 

    res = (int) bf->size;
    release_buf(bf);

    errno = err;
    return res;
}

/*---------------------------------------------------------*/

void dev_aread(dev, offset, size, buf)      /* asynchronous read    */

DEV     dev;
LONG    offset;
INT     size;
BUFFER  *buf;

{
    BUF     *bf  = get_buf(); 
    int     err;
    INT     chan = chan_fetch((char *)bf, (char *)&err);

    bf->size = size;
    bf->buf  = (CHAR *)buf;
    bf->flag = ASYNCH;

    msg.cmd  = READ;
    msg.arg1 = (ARGTYPE) dev;
    msg.arg2 = (ARGTYPE) offset;  
    msg.arg3 = (ARGTYPE) size;
    msg.chan = chan;

    talk();
}

/*---------------------------------------------------------*/

int dev_swrite(dev, offset, buf, size)      /* synchronous write    */

DEV     dev;
LONG    offset;
CHAR    *buf;
INT     size;

{
    BUF     *bf  = get_buf(); 
    int     err;
    int     res;
    INT     chan = chan_fetch((char *)bf, (char *)&err);

    data_out(MAX_PID + major(dev), chan, buf, size);
    bf->flag = SYNCH;

    msg.cmd  = WRITE;
    msg.arg1 = (ARGTYPE) dev;
    msg.arg2 = (ARGTYPE) offset; 
    msg.arg3 = (ARGTYPE) size;
    msg.chan = chan;

    talk();
    chan_sleep(chan); 

    res = (int) bf->size;
    release_buf(bf);

    errno = err;
    return res;
}

/*---------------------------------------------------------*/

void dev_awrite(dev, offset, size, buf)     /* asynchronous write   */

DEV     dev;
LONG    offset;
INT     size;
BUFFER  *buf; 

{
    BUF     *bf  = get_buf(); 
    int     err;
    INT     chan = chan_fetch((char *)bf, (char *)&err);

    data_out(MAX_PID + major(dev), chan, (CHAR *)buf->data, size);
    bf->buf  = (CHAR *)buf;
    bf->flag = ASYNCH;

    msg.cmd  = WRITE;
    msg.arg1 = (ARGTYPE) dev;
    msg.arg2 = (ARGTYPE) offset; 
    msg.arg3 = (ARGTYPE) size;
    msg.chan = chan;

    talk();
}

/*---------------------------------------------------------*/

int dev_ioctl(dev, request, arg)

DEV     dev;
int     request;
char    *arg;

{
    int     err;
    int     res;
    BUF     *bf  = get_buf();
    INT     chan = chan_fetch((char *)bf, (char *)&err);

    bf->buf = (CHAR *)arg;

    msg.cmd  = IOCTL;
    msg.arg1 = (ARGTYPE) dev;
    msg.arg2 = (ARGTYPE) request;
    msg.chan = chan;

    (void) MEMCPY(msg.str1, arg, 64);

    talk();
    chan_sleep(chan);

    res = (int) bf->size;
    release_buf(bf);

    errno = err;
    return res;
}

/*---------------------------------------------------------*/

void dev_op_done(chan, error)

INT     chan;
int     error;

{
    BUF     *bf;
    int     *ep;

    chan_pointers(chan, &bf, &ep);

    *ep       = error;
    bf->errno = error;

    chan_wakeup(chan);
}
    
/*---------------------------------------------------------*/

void dev_rd_done(chan, res, dev, error)

INT     chan;
int     res;
DEV     dev; 
int     error;

{
    BUF     *bf;
    int     *ep;
    BUFFER  *bp;

    chan_pointers(chan, &bf, &ep);

    *ep       = error;
    bf->errno = error;

    if ( bf->flag == SYNCH )
    {
        bf->size = res;  

        if ( bf->size > 0 ) 
            data_in(MAX_PID + major(dev), chan, bf->buf, bf->size); 
 
        chan_wakeup(chan);
    }
    else
    {
        bp = (BUFFER *)(bf->buf);

        if ( error == 0 )
            data_in(MAX_PID + major(dev), chan, (CHAR *)bp->data, bf->size);

        buf_iodone(bp, error);
        release_buf(bf);
        chan_release(chan);
    }
}

/*---------------------------------------------------------*/

void dev_wr_done(chan, res, error)

INT     chan;
int     res;
int     error;

{
    BUF     *bf;
    int     *ep;

    chan_pointers(chan, &bf, &ep);

    *ep       = error;
    bf->size  = res;
    bf->errno = error;

    if ( bf->flag == SYNCH )
        chan_wakeup(chan);
    else
    {
        buf_iodone((BUFFER *)(bf->buf), error);
        release_buf(bf);
        chan_release(chan);
    }
}

/*---------------------------------------------------------*/ 

void dev_ioctl_done(chan, arg, size, error)

INT     chan;
char    *arg;
int     size;
int     error; 

{
    BUF     *bf;
    int     *ep;

    chan_pointers(chan, &bf, &ep);

    *ep       = error;
    bf->size  = size;

    if ( size > 0 && size < 65 )
        (void) MEMCPY(bf->buf, arg, size);
    
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

