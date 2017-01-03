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

#define FM_PID      1       /* the file manager     */
#define MM_PID      2       /* the memory manager   */
#define PM_PID      3       /* the process manager  */
#define IM_PID      4       /* the IPC manager      */
#define INIT_PID    5       /* the init process     */

#define BASE_PID    6       /* the first user pid   */

#define MAX_PID     1024    /* no. of distinct pid  */

#define STD_CHAN    1       /* for cases where no channel is needed */
