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

#define O_RDONLY    0x0000  /* open for reading only        */
#define O_WRONLY    0x0001  /* open for writing only        */
#define O_RDWR      0x0002  /* open for reading and writing */
#define O_NDELAY    0x0004  /* non-blocking mode            */
#define O_APPEND    0x0008  /* append to existing contents  */

#define O_CREAT     0x0100  /* create and open file                    */
#define O_TRUNC     0x0200  /* open and truncate file                  */
#define O_EXCL      0x0400  /* open only if file doesn't already exist */

#define F_DUPFD 0   /* duplicate file descriptor */
#define F_GETFD 1   /* get file descriptor flags */
#define F_SETFD 2   /* set file descriptor flags */
#define F_GETFL 3   /* get file flags            */
#define F_SETFL 4   /* set file flags            */


