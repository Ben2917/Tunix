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
* This module manages the identifiers for IPC.
*
***********************************************************
*/
#include    "../include/id.h"

#define MODULUS     (1 << 16)

static int  next_msg;
static int  next_sem;
static int  next_shm;

/*------------------------------------------------------*/

void    id_init(type)

int     type;

{
    switch ( type )
    {
    case MSG_ID:
        next_msg = 0;
        break;
    case SEM_ID:
        next_sem = 0;
        break;
    case SHM_ID:
        next_shm = 0;
        break;
    }
}

/*------------------------------------------------------*/

int id_new(type)

int     type;

{
    int result;

    switch ( type )
    {
    case MSG_ID:
        result = next_msg++;
        if ( next_msg >= MODULUS )
            next_msg = 0;
        break;
    case SEM_ID:
        result = next_sem++;
        if ( next_sem >= MODULUS )
            next_sem = 0;
        break;
    case SHM_ID:
        result = next_shm++;
        if ( next_shm >= MODULUS )
            next_shm = 0;
        break;
    }

    return result;
}

