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

/* list of possible commands */

#define REPLY       0

#define OPEN        1
#define CREAT       2
#define DUP         3
#define CLOSE       4
#define SYNC        5
#define LSEEK       6
#define READ        7
#define WRITE       8
#define LINK        9
#define UNLINK      10
#define MKNOD       11
#define STAT        12
#define FSTAT       13
#define ACCESS      14
#define MOUNT       15
#define UMOUNT      16
#define PIPE        17
#define UMASK       18
#define CHMOD       19
#define CHOWN       20
#define CHDIR       21
#define CHROOT      22
#define FCNTL       23
#define USTAT       24
#define UTIME       25
#define IOCTL       26

/* commands used by memory manager  */

#define MM_TASKS    100 
 
#define ALLOC       101
#define XALLOC      102
#define FREE        103
#define ATTACH      104
#define DETACH      105
#define GROW        106
#define RDUP        107
#define DUMP        108
#define VFAULT      109 
#define PFAULT      110
#define BLKSDONE    111
#define BLKNODONE   112
#define DUMPDONE    113
#define GETBLKS     114
#define PUTBLKS     115
#define BLKNOS      116
#define MMDONE      117
#define MMADONE     118
#define MMXDONE     119
#define MMGDONE     120
#define AHDRDONE    121

/* commands used only by process manager */

#define PM_TASKS    200

#define FORK        201 
#define EXEC        202 
#define EXIT        203  
#define PAUSE       204
#define WAIT        205
#define GETPPID     206
#define GETUID      207
#define NICE        208
#define SETPGRP     209
#define SETUID      210
#define TIMES       211
#define KILL        212
#define SIGNAL      213
#define ALARM       214
#define BRK         215
#define SLEEP       216
#define WAKEUP      217
#define TICK        218
#define SETEID      219  
#define XSTAT       220
#define XAHDR       221
#define BEGDMP      222
#define ENDDMP      223
#define PMDONE      224
#define PMADONE     225
#define PMDDONE     226
#define PMSDONE     227 
#define PMXDONE     228
#define FAULT_DONE  229
#define ACT_ON_SIG  230
#define IPCDONE     231

/* commands used only by IPC manager    */

#define IPC_TASKS   300

#define MSGCTL      301
#define MSGGET      302
#define MSGSND      303
#define MSGRCV      304
#define SEMCTL      305
#define SEMGET      306
#define SEMOP       307
#define SHMCTL      308
#define SHMGET      309
#define SHMAT       310
#define SHMDT       311
#define SHM_DONE    312
#define SHMAT_DONE  313

/* commands used only by drivers    */

#define DR_TASKS    400

#define OPEN_DONE   401
#define READ_DONE   402
#define WRITE_DONE  403
#define IOCTL_DONE  404
#define INTR        405

/* commands used only in the emulation  */

#define EMUL        500

#define LISTEN      501
#define READ_IN     502
#define WRITE_OUT   503
#define PEEK        504
#define POKE        505
#define INIT        506

/* commands used for kernel requests */

#define KERN        600

#define PANIC               601
#define PTABLE              602
#define PTABLE_DUP          603
#define PTE_BUILD           604
#define PTE_SET_PRSNT       605
#define PTE_SET_NOTPRSNT    606
#define PTE_WRITE           607
#define PTE_NOTWRITE        608
#define PTE_IS_PRSNT        609
#define PTE_ACCESS          610
#define PTE_DIRTY           611
#define PTE_RESET           612
#define PTE_SET_FRAME       613
#define PTE_GET_FRAME       614
#define MEM_COPY            615
#define KMEM_CPIN           616
#define KMEM_CPOUT          617
#define MEM_ZERO            618
#define PD_ALLOC            619
#define PDE_BUILD           620
#define CALL_HDLR           621
#define ADR2IND_OFF         622
