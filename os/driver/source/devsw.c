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
* This module implements the classical device operations.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/fcntl.h"
#include    "../include/pid.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/device.h"
#include    "../include/comm.h"

#define BLOCK_SIZE  1024

extern int  errno;

char logname[] = "swlog";

extern int  open();
extern int  close();
extern int  read();
extern int  write();
extern long lseek();

static int      rfd;
  
static CHAR     iobuf[BLOCK_SIZE]; 
 
#define ERROR(err)  { errno = err; return -1; }

/*---------------------------------------------------*/

void dev_init()

{
}

/*---------------------------------------------------*/

int dev_open(dev)

DEV     dev;

{
    if ( major(dev) == 1 )
    {
        rfd = open("/tmp/swap.dev", O_RDWR | O_CREAT | O_TRUNC, 0700);
        return rfd;
    }
    else
        return  -1; 
}

/*---------------------------------------------------*/

void dev_close(dev)

DEV     dev;

{
    if ( rfd != -1 )
        (void) close(rfd);

    unlink("/tmp/swap.dev");

    rfd = -1;
}

/*---------------------------------------------------*/

int dev_read(dev, offset, nbytes, chan, pid)

DEV     dev;
LONG    offset;
INT     nbytes;
INT     chan;
INT     pid;

{
    INT     amnt, done = 0;

    if ( major(dev) != 1 ) 
        ERROR(EINVAL)

    (void) lseek(rfd, offset, 0);

    while ( done < nbytes )
    {
        amnt = MIN(BLOCK_SIZE, nbytes - done);

        if ( read(rfd, (char *)iobuf, amnt) != amnt )
            return -1;

        data_out(pid, chan, iobuf, amnt);

        done += amnt;
    } 
        
    return 0;
}

/*---------------------------------------------------*/

int dev_write(dev, offset, nbytes, chan)

DEV     dev;
LONG    offset;
INT     nbytes;
INT     chan;

{
    INT     amnt, done = 0;

    if ( major(dev) != 1 )
        ERROR(EINVAL)

    (void) lseek(rfd, offset, 0);

    while ( done < nbytes )
    {
        amnt = MIN(BLOCK_SIZE, nbytes - done);

        data_in(MM_PID, chan, iobuf, amnt);

        if ( write(rfd, (char *)iobuf, amnt) != amnt )
            return -1;

        done += amnt;
    }

    return 0;
}

/*---------------------------------------------------*/

int dev_ioctl(dev, request, arg, szp)

DEV     dev;
int     request;
char    *arg;
int     *szp;

{
    *szp = 0;
    errno = EINVAL;
    return -1;
}

/*---------------------------------------------------*/

void dev_intr(chan)

INT     chan;

{
}

/*---------------------------------------------------*/

void dev_close_all()

{
    if ( rfd != -1 )
        (void) close(rfd);
}
