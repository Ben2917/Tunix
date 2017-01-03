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

void    proc_init();
int     proc_fork();
int     proc_exec();
void    proc_exit();
int     proc_pause();
int     proc_wait();
void    proc_getpid();
void    proc_getuid();
int     proc_nice();
int     proc_setpgrp();
int     proc_setuid();
void    proc_times();
int     proc_kill();
HDLR    proc_signal();
INT     proc_alarm();
int     proc_brk();
int     proc_sleep();
int     proc_wakeup();
int     proc_tick();
void    proc_fdone();
void    proc_mdone();
void    proc_stat();
void    proc_act_on_sig();
void    proc_idone();

