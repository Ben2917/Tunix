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

void    panic();
INT     ptable();
void    ptabl_dup();
void    pte_build();
void    pte_set_prsnt();
void    pte_set_notprsnt();
void    pte_write();
void    pte_notwrite();
int     pte_is_prsnt();
int     pte_dirty();
int     pte_access();
void    pte_reset();
void    pte_set_frame();
INT     pte_get_frame();
void    pde_add();
INT     pd_alloc();
void    adr2ind_off();
void    mem_copy();
void    kmem_cpin();
void    kmem_cpout();
void    mem_zero();

