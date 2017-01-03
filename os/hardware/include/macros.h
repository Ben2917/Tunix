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

#define TRUE    1
#define FALSE   0

extern char*    memcpy();
extern char*    memmove();
extern int      memcmp();
extern char*    memset();
extern int      strcmp();
extern char*    strcpy();
extern char*    strcat();
extern int      strlen();
extern long     time();
extern void     exit();

#define MEMCPY(a, b, n)     memcpy((char *)(a), (char *)(b), (unsigned)(n))
#define MEMMOVE(a, b, n)    memmove((char *)(a), (char *)(b), (unsigned)(n))
#define MEMCMP(a, b, n)     memcmp((char *)(a), (char *)(b), (unsigned)(n))
#define MEMSET(a, c, n)     memset((char *)(a), (int) (c), (unsigned)(n))
#define STRCMP(a, b)        strcmp((char *)(a), (char *)(b))
#define STRLEN(s)           strlen((char *)(s))
#define STRCPY(a, b)        strcpy((char *)(a), (char *)(b))
#define STRCAT(a, b)        strcat((char *)(a), (char *)(b))
#define TIME()              time((long *)0)

#define MIN(a, b)           ((a) < (b)) ? (a) : (b)
#define MAX(a, b)           ((a) > (b)) ? (a) : (b)
