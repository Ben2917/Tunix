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

int     ptable();
void    ptable_dup();
void    pte_build();
void    pte_set_prsnt();
void    pte_set_notprsnt();
void    pte_write();
void    pte_notwrite();
void    pte_is_prsnt();
void    pte_dirty();
void    pte_access();
void    pte_reset();
void    pte_set_frame();
void    pte_get_frame();
void    mem_copy();
void    kmem_cpin();
void    kmem_cpout();
void    kmem_cp_to_buf();
void    mem_zero();
INT     pd_alloc();
void    pde_build();
void    adr2ind_off();
INT     adr2ind();

/*---------- Needed for emulation -------------------*/
int     mem_read();
int     mem_write();
