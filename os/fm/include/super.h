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

void    sup_init();
void    sup_sync();
int     sup_mount();
int     sup_umount();
INODE   *sup_mounted();
INODE   *sup_mnt_point();
INODE   *sup_ialloc();
void    sup_ifree(); 
void    sup_iget();
void    sup_iput();
BUFFER  *sup_alloc();
void    sup_free();
int     sup_ustat();
