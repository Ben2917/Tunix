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
#define MAX_DEV     4

extern int  errno;

extern int  open();
extern int  close();
extern int  read();
extern int  write();
extern long lseek();

char logname[] = "hdlog";

static int      fd[MAX_DEV];
  
static CHAR     iobuf[BLOCK_SIZE]; 
 
#define ERROR(err)  { errno = err; return -1; }

/*---------------------------------------------------*/

void dev_init()

{
    int i;

    for ( i = 0; i < MAX_DEV; ++i )
        fd[i] = -1;
}

/*---------------------------------------------------*/

int dev_open(dev)

DEV     dev;

{
    char    devname[20];

    dev = minor(dev);

    if ( dev >= MAX_DEV )
        ERROR(ENODEV)

    (void) sprintf(devname, "fs%d", dev);

    fd[dev] = open(devname, O_RDWR, 0);

    return 0; 
}

/*---------------------------------------------------*/

void dev_close(dev)

DEV     dev;

{
    dev = minor(dev);

    if ( dev < MAX_DEV && fd[dev] != -1 )
        (void) close(fd[dev]);

    fd[dev] = -1;
}

/*---------------------------------------------------*/

int dev_read(dev, offset, nbytes, chan, pid)

DEV     dev;
LONG    offset;
INT     nbytes;
INT     chan;
INT     pid;

{
    dev = minor(dev);

    if ( dev >= MAX_DEV || fd[dev] == -1 )
        ERROR(EINVAL)

    (void) lseek(fd[dev], offset, 0);

    if ( read(fd[dev], (char *)iobuf, nbytes) == -1 )
        return -1;

    data_out(pid, chan, iobuf, nbytes); 
        
    return nbytes;
}

/*---------------------------------------------------*/

int dev_write(dev, offset, nbytes, chan)

DEV     dev;
LONG    offset;
INT     nbytes;
INT     chan;

{
    dev = minor(dev);

    if ( dev >= MAX_DEV || fd[dev] == -1 )
        ERROR(EINVAL)

    (void) lseek(fd[dev], offset, 0);

    data_in(FM_PID, chan, iobuf, nbytes);

    if ( write(fd[dev], (char *)iobuf, nbytes) == -1 )
        return -1;

    return nbytes;
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
    int     i;

    for ( i = 0; i < MAX_DEV; ++i )
        if ( fd[i] != -1 )
            (void) close(fd[i]);
}
