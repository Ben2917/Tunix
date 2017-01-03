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

#define KTYPES  1

typedef unsigned char  CHAR;
typedef unsigned short SHORT;
typedef unsigned int   INT;
typedef unsigned long  LONG;

typedef SHORT   DEV;
typedef LONG    BLKNO;

#define major(d)            ((SHORT)(((d) >> 8) & 0xff))
#define minor(d)            ((SHORT)((d) & 0xff))
#define device(maj, min)    ((SHORT)(((maj) << 8) + (min)))

#define ROOT_DEV    0
#define ROOT_INO    1
#define ROOT_UID    1
#define ROOT_GID    1

