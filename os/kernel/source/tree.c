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
* This is a program useful for getting started with Tunix.
* It can be used to construct a Tunix file system already
* filled with directories and files -- an exact copy of some
* Unix directory tree whose path is given as command line
* argument.
************************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/stat.h"
#include    "../include/fcntl.h"
#include    "../include/macros.h"

extern char	*strrchr();
extern char *strdup();

int debug = FALSE;

typedef struct
{
    short   ino;
    char    name[14];

} DIR_ENTRY;

struct stat stbuf;
char        buf[16][4096];
char        fbuf[4096];

void    create_dir();
void    create_file();
void    serror();
void    uerror();

/*----------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
	char  *path;
    char  *p;

    if ( stat(argv[1], &stbuf) == -1 )
    {
        printf("Can't stat %s\n", argv[1]);
        exit(1);
    }

    if ( (stbuf.tx_mode & IFMT) != IFDIR )
    {
        printf("%s is not a directory\n", argv[1]);
        exit(2);
    }

    s_init();
    s_chdir("/etc");

    path = strdup(argv[1]);
    p    = strrchr(path, '/');

    if ( p == (char *)0 )
        p = path;
    else
    {
        *p++ = '\0';
        chdir(path);
    }

    create_dir(p, 0);

	s_sync();
    s_exit(0);
}

/*---------------------------------------------------*/

void create_dir(dirname, lvl)

char *dirname;
int  lvl;

{
    int         fd, n, cnt;
    DIR_ENTRY   *dp;
    char        name[15];

    if ( lvl > 15 )
        return;

    if ( s_mknod(dirname, stbuf.tx_mode, 0) == -1 )
        serror("mknod");

    if ( s_chdir(dirname) == -1 )
        serror("chdir");

    if ( (fd = open(dirname, O_RDONLY, 0)) == -1 )
        uerror("open");

    if ( chdir(dirname) == -1 ) 
        uerror("chdir"); 
 
	printf ("creating directory %s\n", dirname);

    while ( (n = read(fd, buf[lvl], 4096)) > 0 )
        for ( cnt = 0, dp = (DIR_ENTRY *)buf[lvl]; cnt < n; ++dp, cnt += 16 )
        {
            if ( dp->ino == 0 || dp->name[0] == '.' )
                continue;

            MEMCPY(name, dp->name, 14);

            if ( stat(name, &stbuf) == -1 )
                uerror("stat");

            if ( (stbuf.tx_mode & IFMT) == IFDIR )
                create_dir(name, lvl + 1);
            else
                create_file(name);
        }

    (void) close(fd);

    if ( chdir("..") == -1 )
        uerror("chdir");

    if ( s_chdir("..") == -1 )
        serror("chdir");
}

/*---------------------------------------------------*/

void create_file(name)

char *name;

{
    int     fdin, fdout, n;

    if ( (fdin = open(name, O_RDONLY, 0)) == -1 )
        uerror("open");

    if ( (fdout = s_open(name, O_WRONLY | O_CREAT | O_TRUNC, stbuf.tx_mode))
                == -1 )
        serror("open");

	printf ("creating file %s\n", name);

    while ( (n = read(fdin, fbuf, 4096)) > 0 )
        if ( s_write(fdout, fbuf, n) != n )
            serror("write");

    (void) close(fdin);
    (void) s_close(fdout);
}

/*---------------------------------------------------*/

void serror(msg)

char *msg;

{
    char    str[80];

    STRCPY(str, "File manager function ");
    STRCAT(str, msg);

    perror(str);
}
/*---------------------------------------------------*/

void uerror(msg)

char *msg;

{
    char    str[80];

    STRCPY(str, "Unix function ");
    STRCAT(str, msg);

    perror(str);
}



