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

#define K_page

#define NR_PAGES    15
#define PAGE_SIZE   4096
#define QUANTUM     4       /* this many pages swapped at once              */
#define LO_WATER    3       /* start swapping at this level of free frames  */
#define HI_WATER    6       /* stop     "      "   "    "    "   "    "     */

/*
    In the following definition we assume that a page is four
    times the size of a disk block in the file system. If 
    this should not be the case, the following definition will
    have to be altered correspondingly.
*/

#define FACTOR  4

/* The type of a page   */

#define DEMAND_ZERO 0       /* fill with zeros on first fault           */
#define DEMAND_FILL 1       /* fill from executable file on first fault */
#define SWAP        2       /* copy on swap device                      */

typedef INT     ADDR;       /* representation used in memory manager    */

