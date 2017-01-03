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
* This module transfers data directly from a device to a user.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/kmsg.h"
#include    "../include/devcall.h"
#include    "../include/comm.h"
#include    "../include/macros.h"

/*---------------------------------------------------*/ 

int dev_ropen(dev)

DEV     dev;

{
    return dev_open(dev); 
}

/*---------------------------------------------------*/

int dev_rread(dev, pid, chan, offset, nbytes)

DEV     dev;
INT     pid;
INT     chan;
LONG    offset;
INT     nbytes;

{
    int     res;
    INT     amnt, done = 0;
    CHAR    buf[DATA_SIZE];

    while ( done < nbytes )
    {
        amnt = MIN(DATA_SIZE, nbytes - done);

        res = dev_sread(dev, offset, buf, amnt);

        if ( res <= 0 )
            break;

        data_out(pid, chan, buf, amnt); 
        
        done += (INT) res; 
 
        if ( res < (int) amnt )
            break;
    }

    return (int) done;
}

/*---------------------------------------------------*/

int dev_rwrite(dev, pid, chan, offset, nbytes)

DEV     dev;
INT     chan;
LONG    offset;
INT     nbytes;

{
    int     res;
    INT     amnt, done = 0;
    CHAR    buf[DATA_SIZE];

    while ( done < nbytes )
    {
        amnt = MIN(DATA_SIZE, nbytes - done);

        data_in(pid, chan, buf, amnt);

        res = dev_swrite(dev, offset, buf, amnt);

        if ( res != (int) amnt )
            break;

        done += (INT) res;
    }

    return (int) done;
}

/*---------------------------------------------------*/

void dev_rclose(dev)

DEV     dev;

{
    dev_close(dev);
}



