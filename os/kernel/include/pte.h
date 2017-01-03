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

typedef INT     PTE;   

/* Types for page table entries     */

#define T_PRESENT   0x001   /* 1 ==> page is present        */
#define T_WRITE     0x002   /* 1 ==> page is writable       */
#define T_USER      0x004   /* 1 ==> access is kernel only  */
#define T_ACCESSED  0x020   /* 1 ==> page has been accessed */
#define T_DIRTY     0x040   /* 1 ==> page has been written  */

#define PTE_PRESENT(pte)    *(pte) |= T_PRESENT
#define NOT_PRESENT(pte)    *(pte) &= ~T_PRESENT
#define PTE_WRITE(pte)      *(pte) |= T_WRITE
#define NOT_WRITE(pte)      *(pte) &= ~T_WRITE
#define PTE_RESET(pte)      *(pte) &= ~(T_ACCESSED | T_DIRTY)
#define PTE_IS_PRSNT(pte)   (*(pte) & T_PRESENT)
#define PTE_ACCESS(pte)     (*(pte) & T_ACCESSED)
#define PTE_DIRTY(pte)      (*(pte) & T_DIRTY)
#define GET_FRAME(pte)      ((INT) (*(pte) >> 12))
#define SET_FRAME(pte, frm) *(pte) = (*(pte) & 0xfff) + (((PTE) (frm)) << 12)
