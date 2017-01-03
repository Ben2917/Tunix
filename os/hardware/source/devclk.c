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
* This module emulates a system clock.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/signal.h"
#include    "../include/ipc.h"
#include    "../include/msg.h"
#include    "../include/kmsg.h"
#include    "../include/fcodes.h"
#include    "../include/pid.h"
#include    "../include/errno.h"
#include    "../include/macros.h"

IMSG    m     = { 1L, 15, 15 };
int     msgsz = sizeof(IMSG) - sizeof(long);
int     msg_i; 

int     quit = FALSE;

void    wake_me();
void    clean_up();

/*-----------------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
    int pid;

    (void) sscanf(argv[1], "%d", &pid);

    msg_i = msgget((key_t) MSGKEY_I, 0777);

    if ( msg_i == -1 )
    {
        (void) printf("Interrupt queue not found\n");
        exit(1);
    }

    (void) signal(SIGALRM, wake_me);
    (void) signal(SIGUSR1, clean_up);

    while ( !quit )
    {
        alarm(2);
        pause();

        if ( quit )
            break;

        (void) msgsnd(msg_i, (struct msgbuf *)&m, msgsz, 0);

        (void) kill(pid, SIGUSR2);
    }

    exit(0);
}

/*--------------------------------------------*/

void wake_me()

{
    (void) signal(SIGALRM, wake_me);
}

/*--------------------------------------------*/

void clean_up()

{
    (void) signal(SIGUSR1, clean_up);
    quit = TRUE;
}
