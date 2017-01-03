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

void    proc_init();
int     proc_umask();
int     proc_chdir();
int     proc_chmod();
int     proc_chown();
int     proc_chroot();
int     proc_mount();
int     proc_umount();
int     proc_open();
int     proc_dup();
int     proc_close();
int     proc_read();
int     proc_write();
long    proc_lseek();
int     proc_link();
int     proc_unlink();
int     proc_access(); 
int     proc_mknod();
int     proc_pipe();
int     proc_sync();
int     proc_stat();
int     proc_fstat();
int     proc_fcntl();
int     proc_ustat();
int     proc_utime();
int     proc_ioctl();
void    proc_fork(); 
int     proc_seteid(); 
void    proc_exit();  
int     proc_exec();
int     proc_ahdr();
int     proc_getblks();
int     proc_putblks();
int     proc_blknos();

