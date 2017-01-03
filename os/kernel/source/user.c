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
* This program simulates a user process permitting one to
* test the operating system by entering commands much as
* one would in the shell.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/ipc.h"
#include    "../include/msg.h"
#include    "../include/sem.h"
#include    "../include/shm.h"
#include    "../include/fcntl.h"
#include    "../include/ustat.h"
#include    "../include/times.h"
#include    "../include/signal.h"
#include    "../include/kmsg.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/termio.h"
#include    <stdio.h>

#define NIL (char *)0

typedef struct
{
    short ino;
    char  name[14];

} DIR_ENTRY;

extern int  errno;
extern int  my_pid;

extern char *s_shmat();
extern char *strtok();
extern char *ctime();

extern HDLR s_signal();

static void     parse();
static void     show_stat();
static void     show_mstat();
static void     show_sstat();
static void     show_shstat();
static void     copy();
static int      type();
static int      oct_dump();
static void     dump();
static int      show_dir();
static void     track_dir();
static void     ls_print();
static void     perm_string();
static char     *build_mv_path();
static void     help();
static void     print_termio();

char        *tok1, *tok2, *tok3, *tok4;
struct stat stbuf;
char        buf[4096];
char        cwd[64];
char        path[64];

struct
{
    long        mtype;
    char        mtext[80];

} mesg;

MSGID   qbuf;
SEMID   sbuf;
SHMID   shbuf;
SEMBUF  sops[10];
SEMBUF  *sop;

SHORT           array[10];

union
{
    int    val;
    SEMID  *buf;
    SHORT  *array;

} arg;

int         debug = FALSE;

void        my_hdlr();

#define SCAN(s, f, p)       (void) sscanf(s, f, p); errno = 0

static char *ftime();

/*-----------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
    char    cmdline[64];
    char    *res;
    int     mode;
    long    key;
    long    typ;
    int     flg;
    int     cmd;
    int     nsems;
    int     qid;
    int     size;
    int     addr;
    int     i;
    int     result;

    SCAN(argv[1], "%d", &my_pid);

    if ( argc > 2 && STRCMP(argv[2], "-d") == 0 )
        debug = TRUE; 
    
    s_init(); 
 
    cwd[0] = '/';
    cwd[1] = '\0';

    for (;;)
    {
        putchar('>');
        while ( (res = gets(cmdline)) == NIL && errno == EINTR )
            ;

        if ( res == NIL )
            break;

        parse(cmdline);

        errno = 0;

        if ( STRCMP(tok1, "help") == 0 )
            help();
        else if ( STRCMP(tok1, "ln") == 0 ) 
        {
            if ( tok3 == NIL )
                printf("Usage: ln old_path new_path\n");
            else if ( s_link(tok2, tok3) == -1 )
            {
                perror("ln");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "rm") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: rm path\n");
            else if ( s_stat(tok2, &stbuf) == -1 )
            {
                perror("rm");
                errno = 0;
            }

            if ( (stbuf.tx_mode & IFMT) == IFDIR )
                printf("%s is a directory; use rmdir\n", tok2);
            else if ( s_unlink(tok2) == -1 )
            {
                perror("rm");
                errno = 0;
            }               
        }
        else if ( STRCMP(tok1, "rmdir") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: rmdir path\n");
            else if ( s_stat(tok2, &stbuf) == -1 )
            {
                perror("rmdir");
                errno = 0;
            }
            else if ( (stbuf.tx_mode & IFMT) != IFDIR )
                printf("%s is not a directory\n", tok2);
            else if ( s_unlink(tok2) == -1 )
            {
                perror("rmdir");
                errno = 0;
            }               
        }
        else if ( STRCMP(tok1, "mount") == 0 )
        {
            if ( tok4 == NIL )
                printf("Usage: mount spec dir rwflag\n");
            else if ( s_mount(tok2, tok3, atoi(tok4)) == -1 )
            {
                perror("mount");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "umount") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: umount spec\n");
            else if ( s_umount(tok2) == -1 )
            {
                perror("umount");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "cd") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: cd path\n");
            else if ( s_chdir(tok2) == -1 )
            {
                perror("cd");
                errno = 0;
            }
            else
                track_dir(tok2);
        }
        else if ( STRCMP(tok1, "ls") == 0 )
        {
            if ( tok2 == NIL )
                tok2 = ".";

            if ( show_dir(tok2) == -1 )
            {
                perror("ls");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "chown") == 0 )
        {
            if ( tok3 == NIL )
                printf("Usage: chown new_owner path\n");
            else if ( s_stat(tok3, &stbuf) == -1 )
            {
                perror("chown");
                errno = 0;
            }
            else if ( s_chown(tok3, atoi(tok2), stbuf.tx_gid) == -1 )
            {
                perror("chown");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "chgrp") == 0 )
        {
            if ( tok3 == NIL )
                printf("Usage: chgrp new_grp path\n");
            else if ( s_stat(tok3, &stbuf) == -1 )
            {
                perror("chgrp");
                errno = 0;
            }
            else if ( s_chown(tok3, stbuf.tx_uid, atoi(tok2)) == -1 )
            {
                perror("chgrp");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "stat") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: stat path\n");
            else if ( s_stat(tok2, &stbuf) == -1 )
            {
                perror("stat");
                errno = 0;
            }
            else
                show_stat(&stbuf);
        } 
        else if ( STRCMP(tok1, "cp") == 0 )
        {
            if ( tok3 == NIL )
                printf("Usage: cp path1 path2\n");
            else copy(tok2, tok3);
        }
        else if ( STRCMP(tok1, "mv") == 0 )
        {
            if ( tok3 == NIL )
            {
                printf("Usage: mv old_path new_path\n");
                continue;
            }

            if ( s_stat(tok3, &stbuf) != -1 &&
                        (stbuf.tx_mode & IFMT) == IFDIR )
                tok3 = build_mv_path(tok2, tok3);
            else
                errno = 0;

            if ( s_link(tok2, tok3) == -1 || s_unlink(tok2) == -1 )
            {
                perror("mv");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "chmod") == 0 )
        {
            if ( tok3 == NIL )
            {
                printf("Usage: chmod mode path\n");
                continue;
            }

            SCAN(tok2, "%o", &mode);

            if ( s_chmod(tok3, mode) == -1 )
            {
                perror("chmod");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "mkdir") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: mkdir path\n");
            else if ( s_mknod(tok2, IFDIR | 0755, 0) == -1 )
            {
                perror("mkdir");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "mknod") == 0 )
        {
            if ( tok4 == NIL )
            {
                printf("Usage: mknod path mode dev\n");
                continue;
            }

            SCAN(tok3, "%o", &mode);

            if ( s_mknod(tok2, mode, atoi(tok4)) == -1 )
            {
                perror("mknod");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "cat") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: cat path\n");
            else if ( type(tok2) == -1 )
            {
                perror("cat");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "od") == 0 )
        {
            int     from = 0;
            int     to   = 0;

            if ( tok2 == NIL )
            {
                printf("Usage: cat path [from [to]]\n");
                continue;
            }

            if ( tok3 != NIL )
            {
                from = atoi(tok3);

                if ( tok4 != NIL )
                    to = atoi(tok4);
            }

            if ( oct_dump(tok2, from, to) == -1 )
            {
                perror("od");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "pwd") == 0 )
        {
            printf("%s\n", cwd);
            continue;  
        }
        else if ( STRCMP(tok1, "utime") == 0 )
        {
            if ( tok2 == NIL )
            {
                printf("Usage: utime path\n");
                continue;
            }

            if ( s_utime(tok2, 0L, 0L) == -1 )
            {
                perror("utime");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "ustat") == 0 )
        {
            struct ustat ubuf;

            if ( tok2 == NIL )
            {
                printf("Usage: ustat device\n");
                continue;
            }

            if ( s_ustat(atoi(tok2), &ubuf) == -1 )
            {
                perror("ustat");
                errno = 0;
            }
            else
                printf("free blocks: %ld, free inodes: %d\n",
                                    ubuf.f_tfree, ubuf.f_tinode);       
        }
        else if ( STRCMP(tok1, "ioctl") == 0 )
        {
            int             fd;
            int             request;
            struct termio   tbuf;

            if ( tok2 == NIL || tok3 == NIL )
            {
                printf("Usage: ioctl path request [arg]\n");
                continue;
            }

            if ( STRCMP(tok3, "TCGETA") == 0 )
                request = TCGETA;
            else if ( STRCMP(tok3, "TCSETAF") == 0 )
                request = TCSETAF;
            else
                request = atoi(tok3);

            if ( request == TCSETAF && tok4 == NIL )
            {
                printf("Usage: ioctl path request (RAW | COOKED)\n");
                continue;
            }

            if ( request == TCSETAF )
                ioctl(0, TCGETA, &tbuf);

            if ( STRCMP(tok4, "RAW") == 0 )
            {
                tbuf.c_oflag &= ~OPOST;
                tbuf.c_lflag &= ~ICANON; 
            };

            fd = s_open(tok2, O_RDONLY, 0);

            if ( fd == -1 )
            {
                perror("open");
                errno = 0;
                continue;
            }

            (void) s_ioctl(fd, request, (char *)&tbuf);
            (void) s_close(fd);

            if ( errno )
            {
                perror("ioctl");
                errno = 0;
            }
            else if ( request == TCGETA )
                print_termio(&tbuf);
        }
        else if ( STRCMP(tok1, "sync") == 0 )
        {
            s_sync();

            if ( errno )
            {
                perror("sync");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "fork") == 0 )
        {
            int pid = s_fork();

            printf("child pid: %d\n", pid);
        }
        else if ( STRCMP(tok1, "exec") == 0 )
        {
            if ( tok2 == NIL )
            {
                printf("Usage: exec path\n");
                continue;
            }

            if ( s_exec(tok2) == -1 )
            {
                perror("utime");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "exit") == 0 )
        {
            if ( tok2 == NIL )
            {
                printf("Usage: exit xval\n");
                continue;
            }

            s_exit(atoi(tok2));
            exit(0);
        }
        else if ( STRCMP(tok1, "pause") == 0 )
        {
            (void) s_pause();
        }
        else if ( STRCMP(tok1, "wait") == 0 )
        {
            INT     stat;
            int     cpid = s_wait(&stat);

            if ( cpid == -1 )
                perror("wait");
            else
                printf("child %d died with status %d\n", cpid, stat);
        }
        else if ( STRCMP(tok1, "getpid") == 0 )
        {
            INT     ppid, pgid;

            s_getpid(&ppid, &pgid);
            printf("parent pid: %d, process group id: %d\n", ppid, pgid);
        }
        else if ( STRCMP(tok1, "getuid") == 0 )
        {
            INT uid, gid, euid, egid;

            s_getuid(&uid, &gid, &euid, &egid);
            printf("uid: %d, gid: %d, euid: %d, egid: %d\n", uid, gid,
                                                            euid, egid);
        }
        else if ( STRCMP(tok1, "nice") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: nice incr\n");
            else if ( s_nice(atoi(tok2)) == -1 )
                perror("nice");
        }
        else if ( STRCMP(tok1, "setpgrp") == 0 )
        {
            (void) s_setpgrp();
        }
        else if ( STRCMP(tok1, "setuid") == 0 )
        {
            if ( tok3 == NIL )
                printf("Usage: setuid new_uid new_gid\n");
            else if ( s_setuid((SHORT)atoi(tok2), (SHORT)atoi(tok3)) == -1 )
                perror("setuid");
        }
        else if ( STRCMP(tok1, "times") == 0 )
        {
            struct tms  tmsbuf;

            s_times(&tmsbuf);
            printf("self: %ld, children: %ld\n",
                            tmsbuf.tms_utime, tmsbuf.tms_cutime);
        }
        else if ( STRCMP(tok1, "kill") == 0 )
        {
            if ( tok3 == NIL )
                printf("Usage: kill pid signal\n");
            else if ( s_kill(atoi(tok2), atoi(tok3)) == -1 )
                perror("kill");
        }
        else if ( STRCMP(tok1, "signal") == 0 )
        {
            int     hnr;
            HDLR    hdlr;

            if ( tok3 == NIL )
                printf("Usage: signal sig hdlr\n");
            else
            {
                hnr = atoi(tok3);
                if ( hnr == 0 )
                    hdlr = SIG_IGN;
                else if ( hnr == 1 )
                    hdlr = SIG_DFL;
                else
                    hdlr = my_hdlr;

                (void) s_signal(atoi(tok2), hdlr);
            }
        }
        else if ( STRCMP(tok1, "alarm") == 0 )
        {
            if ( tok2 == NIL )
                printf("Usage: alarm secs\n");
            else
                printf("Still left: %d\n", s_alarm(atoi(tok2)));
        }
        else if ( STRCMP(tok1, "msgget") == 0 ) 
        {
            if ( tok3 == NIL )
            {
                printf("Usage: msgget key flg\n");
                continue;
            }

            SCAN(tok2, "%ld", &key);
            SCAN(tok3, "%o", &flg);

            if ( (qid = s_msgget(key, flg)) == -1 )
            {
                perror("msgget");
                errno = 0;
            }
            else
                printf("msqid : %d\n", qid);
        }
        else if ( STRCMP(tok1, "msgctl") == 0 )
        {
            if ( tok3 == NIL )
            {
                printf("Usage: msgctl qid cmd\n");
                continue;
            }

            qid = atoi(tok2);

            if ( STRCMP(tok3, "IPC_STAT") == 0 )
                cmd = IPC_STAT;
            else if ( STRCMP(tok3, "IPC_SET") == 0 )
            {
                cmd = IPC_SET;
                printf("Enter uid gid mode qbytes: ");
                gets(cmdline);
                parse(cmdline);
                qbuf.perm.uid = atoi(tok1);
                qbuf.perm.gid = atoi(tok2);
                qbuf.qbytes   = atoi(tok4);
                SCAN(tok3, "%o", &(qbuf.perm.mode));
            }
            else if ( STRCMP(tok3, "IPC_RMID") == 0 )
                cmd = IPC_RMID;
            else
            {
                printf("Unknown command\n");
                continue;
            }

            if ( s_msgctl(qid, cmd, &qbuf) == -1 ) 
            {
                perror("msgctl");
                errno = 0;
            }
            else if ( cmd == IPC_STAT )
                show_mstat();
        }
        else if ( STRCMP(tok1, "msgsnd") == 0 )
        {
            if ( tok4 == NIL )
            {
                printf("Usage: msgsnd qid typ flg\n");
                continue;
            }

            qid = atoi(tok2);

            SCAN(tok3, "%ld", &typ);
            SCAN(tok4, "%o", &flg);

            mesg.mtype = typ;

            gets(mesg.mtext);

            if ( s_msgsnd(qid, &mesg, 80, flg) == -1 )
            {
                perror("msgsnd");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "msgrcv") == 0 )
        {
            if ( tok4 == NIL )
            {
                printf("Usage: msgrcv qid typ flg\n");
                continue;
            }

            qid = atoi(tok2);

            SCAN(tok3, "%ld", &typ);
            SCAN(tok4, "%o", &flg);

            if ( s_msgrcv(qid, &mesg, 80, typ, flg) == -1 )
            {
                perror("msgrcv");
                errno = 0;
            }
            else
            {
                printf("Type is : %ld\n", mesg.mtype);
                printf("%s\n", mesg.mtext);
            }
        }
        else if ( STRCMP(tok1, "semget") == 0 ) 
        {
            if ( tok4 == NIL )
            {
                printf("Usage: semget key nsems flg\n");
                continue;
            }

            nsems = atoi(tok3);
            SCAN(tok2, "%ld", &key);
            SCAN(tok4, "%o", &flg);

            if ( (qid = s_semget(key, nsems, flg)) == -1 )
            {
                perror("semget");
                errno = 0;
            }
            else
                printf("semid : %d\n", qid);
        }
        else if ( STRCMP(tok1, "semctl") == 0 )
        {
            if ( tok3 == NIL )
            {
                printf("Usage: semctl qid cmd\n");
                continue;
            }

            qid   = atoi(tok2);
            nsems = 0;

            if ( STRCMP(tok3, "IPC_STAT") == 0 )
            {
                cmd     = IPC_STAT;
                arg.buf = &sbuf;
            }
            else if ( STRCMP(tok3, "IPC_SET") == 0 )
            {
                cmd = IPC_SET;
                printf("Enter uid gid mode: ");
                gets(cmdline);
                parse(cmdline);
                sbuf.perm.uid = atoi(tok1);
                sbuf.perm.gid = atoi(tok2);
                SCAN(tok3, "%o", &(sbuf.perm.mode));
                arg.buf = &sbuf;
            }
            else if ( STRCMP(tok3, "IPC_RMID") == 0 )
                cmd = IPC_RMID;
            else if ( STRCMP(tok3, "GETNCNT") == 0 )
            {
                cmd = GETNCNT;
                printf("Enter semnum: ");
                gets(cmdline);
                parse(cmdline);
                nsems = atoi(tok1);
            }
            else if ( STRCMP(tok3, "GETPID") == 0 )
            {
                cmd = GETPID;
                printf("Enter semnum: ");
                gets(cmdline);
                parse(cmdline);
                nsems = atoi(tok1);
            }
            else if ( STRCMP(tok3, "GETVAL") == 0 )
            {
                cmd = GETVAL;
                printf("Enter semnum: ");
                gets(cmdline);
                parse(cmdline);
                nsems = atoi(tok1);
            }
            else if ( STRCMP(tok3, "GETALL") == 0 )
            {
                cmd       = GETALL;
                arg.array = array;
                printf("Enter nsems: ");
                gets(cmdline);
                parse(cmdline);
                nsems = atoi(tok1);
            }
            else if ( STRCMP(tok3, "GETZCNT") == 0 )
            {
                cmd = GETZCNT;
                printf("Enter semnum: ");
                gets(cmdline);
                parse(cmdline);
                nsems = atoi(tok1);
            }
            else if ( STRCMP(tok3, "SETVAL") == 0 )
            {
                cmd = SETVAL;
                printf("Enter semnum val: ");
                gets(cmdline);
                parse(cmdline);
                nsems   = atoi(tok1);
                arg.val = atoi(tok2);
            }
            else if ( STRCMP(tok3, "SETALL") == 0 )
            {
                cmd = SETALL;
                printf("Enter nsems: ");
                gets(cmdline);
                parse(cmdline);
                nsems = atoi(tok1);

                for ( i = 0; i < nsems; ++i )
                {
                    printf("Enter value: ");
                    gets(cmdline);
                    parse(cmdline);
                    array[i] = (SHORT) atoi(tok1);
                }
                arg.array = array;
            }
            else
            {
                printf("Unknown command\n");
                continue;
            }

            if ( (result = s_semctl(qid, nsems, cmd, arg)) == -1 )
            {
                perror("semctl");
                errno = 0;
            }
            else switch ( cmd )
            {
            case IPC_SET:
                break;
            case IPC_STAT:
                show_sstat();
                break;
            case IPC_RMID:
                break;
            case GETALL:
                for ( i = 0; i < nsems; ++i )
                    printf("%d ", array[i]);

                printf("\n");
                break;
            case SETVAL:
                break;
            case SETALL:
                break;
            default:
                printf("%d\n", result);
            }
        }
        else if ( STRCMP(tok1, "semop") == 0 )
        {
            if ( tok3 == NIL )
            {
                printf("Usage: semop qid nsops\n");
                continue;
            }

            qid   = atoi(tok2);
            nsems = atoi(tok3);

            for ( i = 0, sop = sops; i < nsems; ++i, ++sop )
            {
                printf("Enter sem_num sem_op sem_flg: ");
                gets(cmdline);
                parse(cmdline);
                sop->num = (short) atoi(tok1);
                sop->op  = (short) atoi(tok2);
                SCAN(tok3, "%ho", &(sop->flg));
            }

            sop = sops;

            if ( s_semop(qid, &sop, nsems) == -1 )
            {
                perror("semop");
                errno = 0;
            }
        }
        else if ( STRCMP(tok1, "shmget") == 0 ) 
        {
            if ( tok4 == NIL )
            {
                printf("Usage: shmget key size flg\n");
                continue;
            }

            SCAN(tok2, "%ld", &key);
            SCAN(tok3, "%lx", &size);
            SCAN(tok4, "%o", &flg);

            if ( (qid = s_shmget(key, size, flg)) == -1 )
            {
                perror("shmget");
                errno = 0;
            }
            else
                printf("shmid : %d\n", qid);
        }
        else if ( STRCMP(tok1, "shmctl") == 0 )
        {
            if ( tok3 == NIL )
            {
                printf("Usage: shmctl shmid cmd\n");
                continue;
            }

            qid = atoi(tok2);

            if ( STRCMP(tok3, "IPC_STAT") == 0 )
                cmd = IPC_STAT;
            else if ( STRCMP(tok3, "IPC_SET") == 0 )
            {
                cmd = IPC_SET;
                printf("Enter uid gid mode: ");
                gets(cmdline);
                parse(cmdline);
                shbuf.perm.uid = atoi(tok1);
                shbuf.perm.gid = atoi(tok2);
                SCAN(tok3, "%o", &(shbuf.perm.mode));
            }
            else if ( STRCMP(tok3, "IPC_RMID") == 0 )
                cmd = IPC_RMID;
            else
            {
                printf("Unknown command\n");
                continue;
            }

            if ( s_shmctl(qid, cmd, &shbuf) == -1 ) 
            {
                perror("shmctl");
                errno = 0;
            }
            else if ( cmd == IPC_STAT )
                show_shstat();
        }
        else if ( STRCMP(tok1, "shmat") == 0 )
        {
            if ( tok4 == NIL )
            {
                printf("Usage: shmat shmid addr flg\n");
                continue;
            }

            qid = atoi(tok2);

            SCAN(tok3, "%x", &addr);
            SCAN(tok4, "%o", &flg);

            if ( (addr = (int) s_shmat(qid, (char *)addr, flg)) == 0 )
            {
                perror("shmat");
                errno = 0;
            }
            else
                printf("Attached address: %x\n", addr);
        }
        else if ( STRCMP(tok1, "shmdt") == 0 )
        {
            if ( tok2 == NIL )
            {
                printf("Usage: shmdt addr\n");
                continue;
            }

            qid = atoi(tok2);

            SCAN(tok2, "%x", &addr);

            if ( s_shmdt(addr) == -1 )
            {
                perror("shmdt");
                errno = 0;
            }
        }
        else
            printf("Unknown command\n");
    }

    putchar('\n');
    s_exit(0);
}

/*-----------------------------------------------*/

static void parse(line)

char *line;

{
    tok1 = tok2 = tok3 = tok4 = NIL;

    if ( (tok1 = strtok(line, " \t")) == NIL )
        return;

    if ( (tok2 = strtok(NIL, " \t")) == NIL )
        return;

    if ( (tok3 = strtok(NIL, " \t")) == NIL )
        return;

    if ( (tok4 = strtok(NIL, " \t")) == NIL )
        return;
}

/*---------------------------------------*/

static void show_stat(stbuf)

struct stat *stbuf;

{
    SHORT   type = stbuf->tx_mode & IFMT;
    DEV     dev  = stbuf->tx_dev;
    DEV     rdev = stbuf->tx_rdev;

    switch ( type )
    {
    case IFIFO:
        printf("type         : named pipe\n");
        break;
    case IFCHR:
        printf("type         : character special file\n");
        break;
    case IFDIR:
        printf("type         : directory\n");
        break;
    case IFBLK:
        printf("type         : block special file\n");
        break;
    case IFREG:
        printf("type         : regular file\n");
        break;
    default:
        printf("type         : unknown\n");
        break;
    }

    printf("permissions  : %o\n", stbuf->tx_mode & 0xfff);
    printf("on device    : major %d,  minor %d\n", major(dev), minor(dev));
    printf("inode nr.    : %d\n", stbuf->tx_ino);
    printf("nr. links    : %d\n", stbuf->tx_nlink);
    printf("user id      : %d\n", stbuf->tx_uid);
    printf("group id     : %d\n", stbuf->tx_gid);

    if ( type == IFCHR || type == IFBLK )
        printf("names device : major %d, minor %d\n", major(rdev), minor(rdev));
    else
        printf("file size    : %ld\n", stbuf->tx_size);

    printf("access time  : %s\n", ftime(&stbuf->tx_atime));
    printf("modify time  : %s\n", ftime(&stbuf->tx_mtime));
    printf("status time  : %s\n", ftime(&stbuf->tx_ctime));
}

/*---------------------------------------*/

static void copy(path1, path2)

char *path1;
char *path2;

{
    int     fdin, fdout, n;

    if ( (fdin = s_open(path1, O_RDONLY, 0)) == -1 )
    {
        perror("open");
        errno = 0;
        return;
    }

    if ( (fdout = s_open(path2, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1 )
    {
        perror("open");
        errno = 0;
        s_close(fdin);
        return;
    }

    while ( (n = s_read(fdin, buf, 4096)) > 0 )
        if ( s_write(fdout, buf, n) != n )
            break;

    if ( errno )
    {
        perror("write");
        errno = 0;
    }

    s_close(fdin);
    s_close(fdout);
}

/*---------------------------------------*/

static int type(path)

char *path;

{
    int     fdin, n;

    if ( (fdin = s_open(path, O_RDONLY, 0)) == -1 )
        return -1;

    while ( (n = s_read(fdin, buf, 4096)) > 0 )
        write(1, buf, n);

    s_close(fdin);

    return ( errno == 0 ) ? 0 : -1;
}

/*---------------------------------------*/

static int oct_dump(path, from, to)

char *path;
int  from;
int  to;

{
    int     fdin, n, addr;

    if ( (fdin = s_open(path, O_RDONLY, 0)) == -1 )
        return -1;

    addr = from;

    if ( s_lseek(fdin, (long) addr, 0) == -1 )
        return -1;

    while ( (n = s_read(fdin, buf, 4096)) > 0 && addr < to )
    {
        dump(n, addr, to);
        addr += n;
    }

    s_close(fdin);

    printf("\n");

    return ( errno == 0 ) ? 0 : -1;
}

/*---------------------------------------*/

static void dump(n, addr, to)

int     n;
int     addr;
int     to;

{
    int     i;
    CHAR    *cp;

    for ( i = 0, cp = (CHAR *)buf; i < n && addr < to; ++addr, ++cp )
    {
        if ( (addr % 16) == 0 )
            printf("\n%07o\t", addr);

        printf("%03o ", *cp);
    }
}

/*---------------------------------------*/

static int show_dir(path)

char *path;

{
    int         fd, n, cnt;
    DIR_ENTRY   *dp;
    char        name[15];

    name[14] = '\0';

    if ( s_stat(path, &stbuf) == -1 )
        return -1;

    if ( (stbuf.tx_mode & IFMT) != IFDIR )
    {
        errno = ENOTDIR;
        return -1;
    }

    if ( (fd = s_open(path, O_RDONLY, 0)) == -1 )
        return -1;

    (void) s_chdir(path);

    while ( (n = s_read(fd, buf, 4096)) > 0 )
        for ( cnt = 0, dp = (DIR_ENTRY *)buf; cnt < n; ++dp, cnt += 16 )
        {
            if ( dp->ino == 0 || dp->name[0] == '.' )
                continue;

            MEMCPY(name, dp->name, 14);

            s_stat(name, &stbuf);

            ls_print(name);
        }

    (void) s_chdir(cwd);

    s_close(fd);

    return 0;
}

/*-----------------------------------------------*/

static void ls_print(name)

char *name;

{
    char    pstr[12];
    char    date[50];
    SHORT   type = stbuf.tx_mode & IFMT;

    (void) cftime(date, "%b %e %y %R", &stbuf.tx_mtime);

    perm_string(pstr, stbuf.tx_mode);

    if ( type == IFCHR || type == IFBLK )
        printf("%s %3d %3d %3d  %4d, %2d %s %s\n", pstr, stbuf.tx_nlink,
                stbuf.tx_uid, stbuf.tx_gid, major(stbuf.tx_rdev),
                minor(stbuf.tx_rdev), date, name);                                    
    else
        printf("%s %3d %3d %3d  %8ld %s %s\n", pstr, stbuf.tx_nlink,
                stbuf.tx_uid, stbuf.tx_gid, stbuf.tx_size,
                date, name);
}

/*-----------------------------------------------*/

static void perm_string(s, mode)

char    *s;
SHORT   mode;

{
    SHORT   type = mode & IFMT;

    if ( type == IFDIR )
        *s++ = 'd';
    else if ( type == IFCHR ) 
        *s++ = 'c';
    else if ( type == IFBLK )
        *s++ = 'b';
    else
        *s++ = '-';

    if ( mode & 0400 )
        *s++ = 'r';
    else
        *s++ = '-';

    if ( mode & 0200 )
        *s++ = 'w';
    else
        *s++ = '-';

    if ( mode & 0100 )
        *s++ = 'x';
    else
        *s++ = '-';

    if ( mode & 040 )
        *s++ = 'r';
    else
        *s++ = '-';

    if ( mode & 020 )
        *s++ = 'w';
    else
        *s++ = '-';

    if ( mode & 010 )
        *s++ = 'x';
    else
        *s++ = '-';

    if ( mode & 04 )
        *s++ = 'r';
    else
        *s++ = '-';

    if ( mode & 02 )
        *s++ = 'w';
    else
        *s++ = '-';

    if ( mode & 01 )
        *s++ = 'x';
    else
        *s++ = '-';

    *s = '\0';
}

/*-----------------------------------------------*/

static void track_dir(path)

char *path;

{
    char    *p, *q, *r;
    char    comp[64];

    if ( path[0] == '/' )           /* absolute path    */
    {
        (void) STRCPY(cwd, path);
        return;
    }

    /* relative path    */

    /* axiom: *p == '/' || *p == '\0'   */

    if ( STRLEN(cwd) > 1 )
        p = cwd + STRLEN(cwd);
    else
        p = cwd;

    q = path;

    for ( ;; )
    {
        for ( r = comp; *q && *q != '/'; ++q )
            *r++ = *q;

        *r = '\0';
        if ( *q == '/' )
            ++q;

        if ( STRLEN(comp) == 0 )
            break;

        if ( STRCMP(comp, ".") == 0 )
            continue;

        if ( STRCMP(comp, "..") == 0 )
        {
            while ( --p > cwd )
                if ( *p == '/' )
                    break;

            if ( p == cwd )
                *(p+1) = '\0';
            else
                *p = '\0';

            continue;
        }

        if ( *p == '\0' )
            (void) STRCPY(p, "/");

        (void) STRCAT(p, comp);

        p = cwd + STRLEN(cwd);
    }
}

/*-----------------------------------------------*/

static char *build_mv_path(old_path, new_path)

char *old_path;
char *new_path;

{
    char    *p = old_path + STRLEN(old_path);

    while ( p > old_path )
        if ( *(--p) == '/' )
            break;

    if ( *p == '/' )
        ++p;

    STRCPY(path, new_path);
    STRCAT(path, "/");
    STRCAT(path, p);

    return path;
}

/*-----------------------------------------------*/

static void my_hdlr()
{
    printf("Signal was caught\n");
}

/*---------------------------------------*/

static void show_mstat()

{
    printf("curr. user id  : %d\n", qbuf.perm.uid);
    printf("curr. grp  id  : %d\n", qbuf.perm.gid);
    printf("creator   uid  : %d\n", qbuf.perm.cuid);
    printf("creator   gid  : %d\n", qbuf.perm.cgid);

    printf("permissions    : %o\n", qbuf.perm.mode);

    printf("no. of messages: %d\n", qbuf.qnum);
    printf("max. no. bytes : %d\n", qbuf.qbytes);
    printf("last snder pid : %d\n", qbuf.lspid);
    printf("last rcver pid : %d\n", qbuf.lrpid);

    printf("last snd time  : %s\n", ftime(&qbuf.stime));
    printf("last rcv time  : %s\n", ftime(&qbuf.rtime));
    printf("last ctl time  : %s\n", ftime(&qbuf.ctime));
}

/*---------------------------------------*/

static void show_sstat()

{
    printf("curr. user id    : %d\n", sbuf.perm.uid);
    printf("curr. grp  id    : %d\n", sbuf.perm.gid);
    printf("creator   uid    : %d\n", sbuf.perm.cuid);
    printf("creator   gid    : %d\n", sbuf.perm.cgid);

    printf("permissions      : %o\n", sbuf.perm.mode);

    printf("no. of semaphores: %d\n", sbuf.nsems);

    printf("last op  time    : %s\n", ftime(&sbuf.otime));
    printf("last ctl time    : %s\n", ftime(&sbuf.ctime));
}

/*---------------------------------------*/

static void show_shstat()

{
    printf("curr. user id    : %d\n", shbuf.perm.uid);
    printf("curr. grp  id    : %d\n", shbuf.perm.gid);
    printf("creator   uid    : %d\n", shbuf.perm.cuid);
    printf("creator   gid    : %d\n", shbuf.perm.cgid);

    printf("permissions      : %o\n", shbuf.perm.mode);

    printf("segmemt size     : %d\n", shbuf.segsz);
    printf("pid of last op   : %d\n", shbuf.lpid);
    printf("pid of creator   : %d\n", shbuf.cpid);
    printf("number attached  : %d\n", shbuf.nattch);

    printf("last attach time : %s\n", ftime(&shbuf.atime));
    printf("last detach time : %s\n", ftime(&shbuf.dtime));
    printf("last ctl    time : %s\n", ftime(&shbuf.ctime));
}

/*-------------------------------------------------------*/ 

static char *ftime(clk)

LONG    *clk;

{
    static char tbuf[60];

    (void) cftime(tbuf, "%b %e %y   %R", clk);

    return tbuf;
}

/*-------------------------------------------------------*/

void panic(s)

char *s;

{
    printf("PANIC: %s\n", s);
}

/*-------------------------------------------------------*/

static char *hlp_list[] =
{
    "ln", "ls", "cd", "cp", "mv", "cat", "pwd", "rm", "mkdir", "rmdir", 
    "od", "mount", "umount", "stat", "chown", "chgrp", "chmod", "mknod",
    "utime", "ustat", "ioctl", "sync", "fork", "exec", "exit", "pause",
    "wait", "nice", "getpid", "getuid", "setuid", "setpgrp", "times",
    "kill", "signal", "alarm", "msgget", "msgctl", "msgsnd", "msgrcv",
    "semget", "semctl", "semop", "shmget", "shmctl", "shmat", "shmdt",
    ""
};

static void help()

{
    char    **lp;
    int     i;

    for ( i = 0, lp = hlp_list; **lp != '\0'; ++i, ++lp )
    {
        if ( (i % 8) == 7 )
            printf("%s\n", *lp);
        else
            printf("%s\t", *lp);
    }

    printf("\n");
}

/*-------------------------------------------------------*/

static void print_termio(tbp)

struct termio   *tbp;

{
    printf("c_iflag: %d\n", (INT) tbp->c_iflag);
    printf("c_oflag: %d\n", (INT) tbp->c_oflag);
    printf("c_cflag: %d\n", (INT) tbp->c_cflag);
    printf("c_lflag: %d\n", (INT) tbp->c_lflag);
    printf("c_line : %c\n", tbp->c_line);
    printf("c_cc   : %d %d %d %d %d %d %d %d\n\n", tbp->c_cc[0], tbp->c_cc[1],
                            tbp->c_cc[2], tbp->c_cc[3], tbp->c_cc[4],
                            tbp->c_cc[5], tbp->c_cc[6], tbp->c_cc[7]);
}
