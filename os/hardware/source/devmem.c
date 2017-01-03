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
* This module emulates system memory.
*
************************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/fcntl.h"
#include    "../include/fault.h"
#include    "../include/ipc.h"
#include    "../include/signal.h"
#include    "../include/kmsg.h"
#include    "../include/pid.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    <stdio.h>

#define NIL (char *)0

extern int  errno;

extern char *strtok();

void    parse();
int     s_peek();
int     s_poke();
void    s_init();
void    mem_inter();
void    clean_up();

char        *tok1, *tok2;

int         quit  = FALSE;
int         debug = TRUE;
int         fdl   = -1;
int         my_pid; 
int         kpid; 

int         msg_k;
int         msg_u;
int         msg_i;

MSG         msg;

#define SCAN(s, f, p)       (void) sscanf(s, f, p); errno = 0

#define PEEK_INTER  16
#define POKE_INTER  17

/*-----------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
    char    cmdline[64];
    int     mode;

    my_pid = MAX_PID + 2;

    SCAN(argv[1], "%d", &kpid);

    errno = 0;

    (void) signal(SIGUSR1, clean_up);

    s_init(); 
 
    while ( putchar('>'), gets(cmdline) != NIL && !quit )
    {
        parse(cmdline);

        errno = 0;

        if ( STRCMP(tok1, "peek") == 0 )
        {
            LONG    addr;
            SHORT   byte;

            if ( tok2 == NIL )
                printf("Usage: peek address\n");
            else
            {
                SCAN(tok2, "%lx", &addr);
                if ( s_peek(addr, &byte) == -1 )
                    printf("peek didn't work\n");
                else
                    printf("Byte: %x\n", byte);
            }
        }
        else if ( STRCMP(tok1, "poke") == 0 )
        {
            LONG    addr;

            if ( tok2 == NIL )
                printf("Usage: poke address\n");
            else
            {
                SCAN(tok2, "%lx", &addr);
                if ( s_poke(addr) == -1 )
                    printf("poke didn't work\n");
            }
        }
        else
            printf("Unknown command\n");
    }

    if ( fdl != -1 )
        (void) close(fdl);

    putchar('\n');
}

/*-----------------------------------------------*/

void parse(line)

char *line;

{
    tok1 = tok2 = NIL;

    if ( (tok1 = strtok(line, " \t")) == NIL )
        return;

    tok2 = strtok(NIL, " \t");
}

/*-----------------------------------------------*/

int s_peek(addr, byte)

LONG    addr;
SHORT   *byte;

{
    mem_inter(PEEK_INTER, (INT) addr);

    if ( (int) msg.arg1 == 0 )
    {
        *byte = (SHORT) msg.arg2;
        return 0;
    }
    else if ( (int) msg.arg1 == -1 )
        return -1;

    mem_inter(VALIDITY, (INT) addr);
    mem_inter(PEEK_INTER, (INT) addr);

    if ( (int) msg.arg1 == 0 )
    {
        *byte = (SHORT) msg.arg2;
        return 0;
    }
    else if ( (int) msg.arg1 == -1 )
        return -1;

    mem_inter(PROTECTION, (INT) addr);
    mem_inter(PEEK_INTER, (INT) addr);

    if ( (int) msg.arg1 == 0 )
    {
        *byte = (SHORT) msg.arg2;
        return 0;
    }

    return -1;
}

/*-----------------------------------------------*/

int s_poke(addr)

LONG    addr;

{
    mem_inter(POKE_INTER, (INT) addr);

    if ( (int) msg.arg1 == 0 )
        return 0;
    else if ( (int) msg.arg1 == -1 )
        return -1;

    mem_inter(VALIDITY, (INT) addr); 
    mem_inter(POKE_INTER, (INT) addr);

    if ( (int) msg.arg1 == 0 )
        return 0;
    else if ( (int) msg.arg1 == -1 )
        return -1;

    mem_inter(PROTECTION, (INT) addr);
    mem_inter(POKE_INTER, (INT) addr);

    return (int) msg.arg1;
}

/*-----------------------------------------------*/

void s_init()

{
    if ( debug )
        fdl = open("memlog", O_WRONLY | O_CREAT | O_TRUNC, 0700);

    msg_k  = msgget((key_t) MSGKEY_K, 0777); 
    msg_u  = msgget((key_t) MSGKEY_U, 0777); 
    msg_i  = msgget((key_t) MSGKEY_I, 0777);

    if ( errno != 0 && errno != EINTR )
        perror("user: s_init");
}

/*-----------------------------------------------*/

void mem_inter(fault, addr)

INT     fault;
INT     addr;

{
    IMSG    m;

    m.mtype  = 1L;
    m.int_no = fault;
    m.arg    = addr;

    (void) msgsnd(msg_i, (struct msgbuf *)&m, sizeof(IMSG) - sizeof(long), 0);

    (void) kill(kpid, SIGUSR2);

    (void) msgrcv(msg_u, (struct msgbuf *)&msg,
                    sizeof(MSG) - sizeof(long), (long) (MAX_PID + 2), 0); 

    if ( fdl != -1 )
        (void) write(fdl, (char *)&msg, sizeof(MSG));
}

/*--------------------------------------------*/

void clean_up()

{
    (void) signal(SIGUSR1, clean_up);
    quit = TRUE;
}

