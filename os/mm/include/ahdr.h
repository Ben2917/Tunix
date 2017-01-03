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

#ifndef K_page
#include    "../include/page.h"
#endif

typedef struct
{
    BLKNO   fblk_text;
    BLKNO   fblk_data; 
    INT     size_text;
    INT     size_data; 
    INT     size_bss; 
    ADDR    vadr_text;
    ADDR    vadr_data;
    ADDR    vadr_bss;

} AHDR;
