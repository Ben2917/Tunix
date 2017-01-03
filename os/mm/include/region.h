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

/* Types for regions    */

#define R_FREE      0
#define R_TEXT      1
#define R_DATA_W    2
#define R_DATA_R    3 
#define R_BSS       4
#define R_STACK     5
#define R_SHM_R     6
#define R_SHM_W     7

#define NR_REGIONS  128

void    reg_init();
int     reg_alloc();
int     reg_xalloc();
void    reg_free();
void    reg_attach();
void    reg_detach();
INT     reg_grow();
int     reg_dup();
int     reg_dump();
int     reg_vfault();
int     reg_pfault();
void    reg_iodone();
void    reg_blkdone();
void    reg_dumpdone();
void    reg_test();
SHORT   reg_peek();
void    reg_poke();
