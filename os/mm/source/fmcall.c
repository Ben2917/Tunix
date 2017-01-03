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
* This module manages the communication with the file
* manager.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/ahdr.h"
#include    "../include/comm.h"
#include    "../include/fmcall.h"
#include    "../include/kcall.h"
#include    "../include/fcodes.h"
#include    "../include/kmsg.h"
#include    "../include/pid.h"
#include    "../include/chan.h"
#include    "../include/macros.h"

extern int      errno;

static MSG      msg;              

#define NBLKS       (PAGE_SIZE / sizeof(BLKNO))

static LONG     blkfid;
static INT      nrblks;
static BLKNO    fstblk;
static INT      rest;
static INT      now;
static INT      blkcnt;
static BLKNO    *blkp;
static BLKNO    bbuf[NBLKS];

/*------------------ Definitions of public functions ------------------*/

int fm_fill(frm_nr, dev, blkp)

INT     frm_nr;
DEV     dev;
BLKNO   *blkp;

{
    int res;
    int err;
    INT chan = chan_fetch((char *)&res, (char *)&err);

    msg.cmd  = GETBLKS;
    msg.arg1 = (ARGTYPE) frm_nr; 
    msg.arg2 = (ARGTYPE) dev;
    msg.arg3 = blkp[0];
    msg.arg4 = blkp[1];
    msg.arg5 = blkp[2];
    msg.arg6 = blkp[3];
    msg.chan = chan; 

    send(FM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return res;
}

/*---------------------------------------------*/

int fm_exec(path, pid, fidp)

char    *path;
INT     pid;
LONG    *fidp;

{
    int err;
    INT chan = chan_fetch((char *)fidp, (char *)&err);

    msg.cmd  = EXEC;
    msg.chan = chan;
    msg.arg1 = (ARGTYPE) pid;

    (void) STRCPY(msg.str1, path); 
 
    send(FM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return ( err ) ? -1 : 0;
}

/*---------------------------------------------*/

void fm_xfinish(chan, fid, error)

INT     chan;
LONG    fid;
int     error;

{
    LONG    *lp;
    int     *ep;

    chan_pointers(chan, (char **)&lp, (char **)&ep);

    *lp = fid;
    *ep = error;

    chan_wakeup(chan);
}

/*---------------------------------------------------------*/

void fm_done(chan, res, error)

INT     chan;
int     res;
int     error;

{
    int     *rp;
    int     *ep;

    chan_pointers(chan, (char **)&rp, (char **)&ep);

    *rp = res;
    *ep = error;
    chan_wakeup(chan);
}

/*---------------------------------------------------------*/

void fm_iodone(frm_nr, chan, res, error)

INT     frm_nr; 
INT     chan;
int     res;
int     error; 

{
    int     *rp;
    int     *ep;

    chan_pointers(chan, (char **)&rp, (char **)&ep);

    *rp = res;
    *ep = error;

    kmem_cpin(frm_nr, chan);

    chan_wakeup(chan);
}

/*---------------------------------------------------------*/

int fm_dump(pid, fd, frm_nr)

INT     pid;
int     fd;
INT     frm_nr;

{
    int     res;
    int     err;
    INT     chan = chan_fetch((char *)&res, (char *)&err);

    kmem_cpout(frm_nr, chan);

    msg.cmd  = PUTBLKS;
    msg.arg1 = (ARGTYPE) pid;
    msg.arg2 = (ARGTYPE) fd;
    msg.chan  = chan;

    send(FM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return res;
}

/*---------------------------------------------------------*/

int fm_blocks(fid, fst_blk, nblks)

LONG    fid;
BLKNO   fst_blk;
INT     nblks;

{
    int     res;
    int     err;
    INT     chan = chan_fetch((char *)&res, (char *)&err);

    now  = MIN(nblks, NBLKS); 

    msg.cmd  = BLKNOS;
    msg.chan = chan; 
    msg.arg1 = (ARGTYPE) fid;
    msg.arg2 = (ARGTYPE) fst_blk;
    msg.arg3 = (ARGTYPE) now;

    nrblks = nblks;
    blkfid = fid;
    fstblk = fst_blk + now;  
    rest   = nblks - now; 
    blkcnt = 0; 
    blkp   = bbuf;

    send(FM_PID, &msg, UNCRITICAL); 
    chan_sleep(chan);

    errno = err; 
    return res;
}

/*---------------------------------------------------------*/

BLKNO fm_nxtblk()

{
    int     res;
    int     err;
    INT     chan;

    if ( blkcnt >= nrblks ) 
        return (BLKNO)0; 
 
    if ( blkp >= bbuf + NBLKS )
    {
        chan = chan_fetch((char *)&res, (char *)&err);

        now   = MIN(rest, NBLKS);
        rest -= now;

        msg.cmd  = BLKNOS;
        msg.chan = chan; 
        msg.arg1 = (ARGTYPE) blkfid;
        msg.arg2 = (ARGTYPE) fstblk;
        msg.arg3 = (ARGTYPE) now;

        fstblk += now;
        blkp    = bbuf;
   
        send(FM_PID, &msg, UNCRITICAL); 
        chan_sleep(chan);

        errno = err;
    }

    ++blkcnt;
    return *blkp++;
}

/*---------------------------------------------*/

int fm_ahdr(pid, ahdrp)

INT     pid;
AHDR    *ahdrp;

{
    int err;
    INT chan = chan_fetch((char *)ahdrp, (char *)&err);

    msg.cmd  = XAHDR;
    msg.chan = chan;
    msg.arg1 = (ARGTYPE) pid;

    send(FM_PID, &msg, UNCRITICAL);
    chan_sleep(chan);

    errno = err;
    return ( err ) ? -1 : 0;
}

/*---------------------------------------------------------*/

void fm_ahdr_done(chan, cp, error)

INT     chan;
char    *cp;
int     error;

{
    AHDR    *ap;
    int     *ep;

    chan_pointers(chan, (char **)&ap, (char **)&ep);

    (void) MEMCPY(ap, cp, sizeof(AHDR));

    *ep = error;

    chan_wakeup(chan);
}

/*---------------------------------------------------------*/

void fm_blkdone(chan, res, error)

INT chan;
int res;
int error;

{
    int *rp;
    int *ep;

    chan_pointers(chan, (char **)&rp, (char **)&ep);

    *rp = res;
    *ep = error;

    data_in(FM_PID, chan, (CHAR *)bbuf, now * sizeof(BLKNO));
    chan_wakeup(chan);
}


