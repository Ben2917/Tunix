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

#define SIGHUP  1   /* hangup                             */
#define SIGINT  2   /* interrupt                          */
#define SIGQUIT 3   /* quit                               */
#define SIGILL  4   /* illegal instruction                */
#define SIGTRAP 5   /* trace trap                         */
#define SIGIOT  6   /* IOT instruction                    */
#define SIGABRT SIGIOT
#define SIGEMT  7   /* EMT instruction                    */
#define SIGFPE  8   /* floating point exception           */
#define SIGKILL 9   /* kill                               */
#define SIGBUS  10  /* bus error                          */
#define SIGSEGV 11  /* segmentation violation             */
#define SIGSYS  12  /* bad argument to system call        */
#define SIGPIPE 13  /* write to a pipe with no readers    */
#define SIGALRM 14  /* alarm clock                        */
#define SIGTERM 15  /* software termination signal        */
#define SIGUSR1 16  /* user defined signal 1              */
#define SIGUSR2 17  /* user defined signal 2              */
#define SIGCLD  18  /* death of a child                   */
#define SIGPWR  19  /* power-fail restart                 */
#define SIGPOLL 20  /* a polled event ocurred             */

#define NSIG    21

#define MAXSIG  32

typedef void (*HDLR)();

#define SIG_DFL (void (*)())0
#define SIG_IGN (void (*)())1
#define SIG_HDL (void (*)())2       /* used by PM   */

#define raise(sig)  kill(getpid(), sig)

void (*signal())(); 
void (*sigset())();
int  ( *ssignal())();

