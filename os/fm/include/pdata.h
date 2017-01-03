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

#ifndef KTYPES
#include    "../include/ktypes.h"
#endif

typedef struct          /* process specific information (u area)    */
{
    SHORT   uid;        /* effective uid of process */
    SHORT   gid;        /* effective gid  "    "    */
    DEV     rdev;       /* root device    "    "    */
    SHORT   rino;       /* root inode no. "    "    */
    DEV     cdev;       /* curr. dev.     "    "    */
    SHORT   cino;       /* curr. ino. no. "    "    */

} PDATA;


