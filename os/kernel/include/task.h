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

#define USR_TASKS   128 
 
#define USER_TASK   1
#define HD_TASK     USR_TASKS       /* devhd    */
#define SW_TASK     HD_TASK + 1     /* devsw    */
#define TY_TASK     SW_TASK + 1     /* devtty   */
#define FM_TASK     TY_TASK + 1     /* fm       */
#define MM_TASK     FM_TASK + 1     /* mm       */
#define IM_TASK     MM_TASK + 1     /* ipcm     */
#define PM_TASK     IM_TASK + 1     /* pm       */

#define SYS_TASKS   PM_TASK - USR_TASKS + 1 
#define ALL_TASKS   USR_TASKS + SYS_TASKS 

void    task_user();
void    task_system();
INT     task_next_sys();
void    task_sig_hdlr();
