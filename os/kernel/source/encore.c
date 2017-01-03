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
*
* This is a utility program for inspecting the log files.
*
************************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/fcntl.h"
#include    "../include/fcodes.h"
#include    "../include/kmsg.h"
#include    "../include/pid.h"

char    *fcodes[] =
{
    "reply", "open", "creat", "dup", "close", "sync", "lseek", "read",
    "write", "link", "unlink", "mknod", "stat", "fstat", "access",
    "mount", "umount", "pipe", "umask", "chmod", "chown", "chdir", "chroot",
    "fcntl", "ustat", "utime", "ioctl"
};

char    *mcodes[] =
{
    "null", "alloc", "xalloc", "free", "attach", "detach", "grow", "rdup",
    "dump", "vfault", "pfault", "blksdone", "blknodone", "dumpdone",
    "getblks", "putblks", "blknos", "mmdone", "mmadone", "mmxdone",
    "mmgdone", "ahdrdone"
};

char    *pcodes[] =
{
    "null", "fork", "exec", "exit", "pause", "wait", "getpid", "getuid",
    "nice", "setpgrp", "setuid", "times", "kill", "signal", "alarm", "brk",
    "sleep", "wakeup", "tick", "seteid", "xstat", "xahdr", "begdmp", "enddmp",
    "pmdone", "pmadone", "pmddone", "pmsdone", "pmxdone", "fault_done",
    "act_on_sig", "ipc_done"
};

char    *icodes[] =
{
    "null", "msgctl", "msgget", "msgsnd", "msgrcv", "semctl", "semget",
    "semop", "shmctl", "shmget", "shmat", "shmdt", "shmdone", "shmatdone"
};

char    *dcodes[] =
{
    "null", "open_done", "read_done", "write_done", "ioctl_done", "intr"
};

char    *ecodes[] = 
{ 
    "null", "listen", "read_in", "write_out", "peek", "poke", "init" 
}; 
 
char    *kcodes[] = 
{
    "null", "panic", "ptable", "ptable_dup", "pte_build", "pte_set_prsnt",
    "pte_set_notprsnt", "pte_write", "pte_notwrite", "pte_is_prsnt",
    "pte_access", "pte_dirty", "pte_reset", "pte_set_frame", "pte_get_frame",
    "mem_copy", "kmem_cpin", "kmem_cpout", "mem_zero", "pd_alloc",
    "pde_build", "call_hdlr", "adr2ind_off"
};

int fdl;
MSG msg;

/*-----------------------------------------------------------------*/
 
main(argc, argv)

int  argc;
char **argv;

{
    char *cmd, ans[20]; 
    char logfile[80];
    int  cnt = 0;

    if ( argc < 2 )
        fdl = open("klog", O_RDONLY, 0600);
    else
    {
        fdl = open(argv [1], O_RDONLY, 0600);
    }

    printf("         command class queue  pid chan errno      arg1      arg2      arg3\n"); 
    printf("--------------------------------------------------------------------------\n"); 

    while ( read(fdl, (char *)&msg, sizeof(MSG)) > 0 )
    {
        if ( msg.queue >= INIT_PID && msg.queue < MAX_PID )
            cmd = "reply";
        else if ( msg.cmd < MM_TASKS )
            cmd = fcodes[msg.cmd];
        else if ( msg.cmd < PM_TASKS )
            cmd = mcodes[msg.cmd - MM_TASKS];
        else if ( msg.cmd < IPC_TASKS )
            cmd = pcodes[msg.cmd - PM_TASKS];
        else if ( msg.cmd < DR_TASKS )
            cmd = icodes[msg.cmd - IPC_TASKS];
        else if ( msg.cmd < EMUL )
            cmd = dcodes[msg.cmd - DR_TASKS];
        else if ( msg.cmd < KERN )
            cmd = ecodes[msg.cmd - EMUL];
        else
            cmd = kcodes[msg.cmd - KERN];

        printf("%16s  %4d  %4d %4d %4d  %4d %9ld %9ld %9ld\n",
            cmd, msg.class, msg.queue, msg.pid, msg.chan, msg.errno,
            (long) msg.arg1, (long) msg.arg2, (long) msg.arg3);

        if ( ++cnt >= 22 )
        {
            (void) gets(ans);
            if ( ans[0] == 'q' )
                break;
            
            cnt = 0;
            printf("         command class queue  pid chan errno      arg1      arg2      arg3\n"); 
            printf("--------------------------------------------------------------------------\n"); 
        }
    }

    (void) close(fdl);
    exit(0);
}
