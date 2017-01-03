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

void    sys_init();
int     sys_open();
void    sys_close();
long    sys_lseek();
int     sys_link();
int     sys_unlink();
int     sys_read();
int     sys_write();
int     sys_access();
int     sys_mknod();
int     sys_mount();
int     sys_umount();
int     sys_stat();
void    sys_fstat();
int     sys_pipe();
int     sys_chmod();
int     sys_chown();
int     sys_chdir();
int     sys_chroot();
void    sys_sync();
int     sys_fcntl();
int     sys_ustat();
int     sys_utime();
int     sys_ioctl();
void    sys_bind();
void    sys_unbind();
void    sys_incref();
int     sys_exec();
int     sys_ahdr();
void    sys_getblks();
void    sys_blknos();
