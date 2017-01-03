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
* This module implements the classical device operations.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/fcntl.h"
#include    "../include/pid.h"
#include    "../include/errno.h"
#include    "../include/macros.h"
#include    "../include/device.h"
#include    "../include/comm.h"
#include    "../include/termio.h"

#define MAX_DEV     1
#define CBLK_SIZE   64
#define MAX_BLOCKS  1000
#define BLOCK_SIZE  1024

#define EOF         0x04            /* ctrl-D   */
#define BACKSPACE   0x08
#define TAB         0x09

#define TAB_SIZE    4               /* how many blanks per tab? */

#define STDIN       0
#define STDOUT      1

extern int  errno;

extern int  open();
extern int  close();
extern int  read();
extern int  write();

char logname[] = "ttylog";

typedef struct cblock
{
    struct cblock   *next;
    INT             in;
    INT             out;
    char            data[CBLK_SIZE];

} CBLOCK;
 
typedef struct
{
    INT     count;
    CBLOCK  *first;
    CBLOCK  *last;

} CLIST;

static CLIST    in_raw;
static CLIST    in_canon;
static CLIST    out;

static int      end_of_file = FALSE;

static CBLOCK   blocks[MAX_BLOCKS];
static char     iobuf[BLOCK_SIZE]; 

static struct termio    tbufsave;
static struct termio    my_buf;

static int      read_some();
static int      line_discipline();
static void     translate_out();
static int      put_in();
static void     canon_out();
static CBLOCK   *cblk_alloc();
static void     cblk_free();
static void     setraw();
static void     unsetraw();

#define ERROR(err)  { errno = err; return -1; }  

static void print_termio();

/*---------------------------------------------------*/

void dev_init()

{
    int     i;
    CBLOCK  *cbp;

    for ( i = 0, cbp = blocks; i < MAX_BLOCKS; ++i, ++cbp )
        cbp->out = CBLK_SIZE;

    in_raw.count = 0;
    in_raw.first = in_raw.last = (CBLOCK *)0;

    in_canon.count = 0;
    in_canon.first = in_canon.last = (CBLOCK *)0;

    setraw();  
    (void) MEMCPY(&my_buf, &tbufsave, sizeof(struct termio));
}

/*---------------------------------------------------*/

int dev_open(dev)

DEV     dev;

{
    return 0;
}

/*---------------------------------------------------*/

void dev_close(dev)

DEV     dev;

{
}

/*---------------------------------------------------*/

int dev_read(dev, offset, nbytes, chan, pid)

DEV     dev;
LONG    offset;
INT     nbytes;
INT     chan;
INT     pid;

{
    INT     done;
    char    *from, *to;
    CBLOCK  *cbp;

    if ( minor(dev) >= (SHORT)MAX_DEV )
        ERROR(EINVAL)

    if ( end_of_file )
    {
        end_of_file = FALSE;
        return 0;
    }

    while ( in_canon.count == 0 )
    {
        if ( in_raw.count == 0 && read_some() == -1 )
            ERROR(EIO)

        if ( line_discipline() == -1 )
            ERROR(EIO)      /* not really honest    */
    }

    cbp = in_canon.first;
    from = &(cbp->data[cbp->out]);
    to   = iobuf;
    done = 0;

    while ( done < nbytes && in_canon.count > 0 )
    {
        *to++ = *from++;
        ++cbp->out;
        ++done; 
        --in_canon.count;

        if ( cbp->out >= cbp->in )
        {
            in_canon.first = cbp->next;
            cblk_free(cbp);
            cbp  = in_canon.first;
            from = &(cbp->data[cbp->out]);
        }

        if ( in_canon.count == 0 )
            in_canon.last = (CBLOCK *)0;
    }

    if ( done > 0 )
        data_out(pid, chan, iobuf, done); 
    else
        end_of_file = FALSE;
        
    return done;
}

/*---------------------------------------------------*/

int dev_write(dev, offset, nbytes, chan)

DEV     dev;
LONG    offset;
INT     nbytes;
INT     chan;

{
    CBLOCK  *cbp;

    if ( minor(dev) >= (SHORT)MAX_DEV )
        ERROR(EINVAL)

    data_in(FM_PID, chan, iobuf, nbytes);

    if ( my_buf.c_oflag & OPOST )
    {
        translate_out(nbytes);

        while ( out.count > 0 )
        {
            cbp = out.first;

            if ( write(STDOUT, cbp->data, cbp->in) == -1 )
                ERROR(EIO)

            out.count -= cbp->in;

            out.first = cbp->next;
            cblk_free(cbp);

            if ( out.count == 0 )
                out.last = (CBLOCK *)0;
        }
    }
    else if ( write(STDOUT, iobuf, nbytes) != nbytes )
        ERROR(EIO)

    return nbytes;
}

/*---------------------------------------------------*/

int dev_ioctl(dev, request, arg, szp)

DEV     dev;
int     request;
char    *arg;
int     *szp;

{
    switch ( request )
    {
    case TCGETA:
        (void) MEMCPY(arg, &my_buf, sizeof(struct termio));
        *szp = sizeof(struct termio);
        return 0;
    case TCSETAF:
        (void) MEMCPY(&my_buf, arg, sizeof(struct termio));
        *szp = sizeof(struct termio);
        return 0;
    default:
        *szp = 0;
        errno = EINVAL;
        return -1;
    }
}

/*---------------------------------------------------*/

void dev_intr(chan)

INT     chan;

{
}

/*---------------------------------------------------*/

void dev_close_all()

{
    unsetraw(); 
}

/*---------------------------------------------------*/

static int read_some()

{
    CBLOCK  *cbp;
    char    c;
    int     more = TRUE;

    do
    {
        cbp = cblk_alloc();

        if ( cbp == (CBLOCK *)0 )
            return -1;

        cbp->in   = 0;
        cbp->out  = 0;
        cbp->next = (CBLOCK *)0;

        if ( in_raw.last == (CBLOCK *)0 )
            in_raw.first = cbp;
        else
            in_raw.last->next = cbp;

        in_raw.last = cbp;

        while ( more && cbp->in < CBLK_SIZE )
        {
            if ( read(STDIN, &c, 1) == -1 )
                return -1;

            cbp->data[(cbp->in)++] = c;
            ++in_raw.count;

            if ( c == '\r' )
            {
                c = '\n';
                (void) write(STDOUT, &c, 1);    /* echo needs \n as well */
            }

            if ( c == '\n' || c == EOF )
                more = FALSE;
        }

    } while ( more ); 

    return 0;
}

/*---------------------------------------------------*/

static int line_discipline()

{
    CBLOCK  *rbp;
    char    c;

    rbp = in_raw.first;

    do
    {
        c = rbp->data[(rbp->out)++];
        --in_raw.count;

        if ( rbp->out == rbp->in )
        {
            in_raw.first = rbp->next;
            cblk_free(rbp);
            rbp = in_raw.first;

            if ( in_raw.first == (CBLOCK *)0 )
                in_raw.last = (CBLOCK *)0;
        }

        if ( my_buf.c_iflag & ICANON )
            switch ( c )
            {
            case EOF:
                end_of_file = TRUE;
                break;
            case BACKSPACE:
                canon_out();
                break;
            case '\r':
                if ( put_in(&in_canon, '\n') == -1 )
                    return -1;
                break;
            default:
                if ( put_in(&in_canon, c) == -1 )
                    return -1;
                break;
            }
        else
            put_in(&in_canon, c);

    } while ( c != '\r' && c != '\n' && c != EOF && in_raw.count != 0 );

    return 0;
}

/*---------------------------------------------------*/

static void translate_out(n)

INT     n;

{
    char    *cp;
    INT     ct, i;

    for ( ct = 0, cp = iobuf; ct < n; ++ct, ++cp )
    {
        switch ( *cp )
        {
        case TAB:
            for ( i = 0; i < TAB_SIZE; ++i )
                put_in(&out, ' ');
            break;
        case '\n':
            put_in(&out, '\r');
            put_in(&out, '\n');
            break;
        default:
            put_in(&out, *cp);
            break;
        }
    }
}

/*---------------------------------------------------*/

static int put_in(clp, ch)

CLIST   *clp;
char    ch;

{
    CBLOCK  *cbp = clp->last;

    if ( cbp == (CBLOCK *)0 || cbp->in >= CBLK_SIZE )
    {
        if ( (cbp = cblk_alloc()) == (CBLOCK *)0 )
            return -1;

        cbp->in   = 0;
        cbp->out  = 0;
        cbp->next = (CBLOCK *)0;

        if ( clp->last == (CBLOCK *)0 )
            clp->first = cbp;
        else
            clp->last->next = cbp;

        clp->last = cbp;
    }

    cbp->data[(cbp->in)++] = ch;
    ++(clp->count);

    return 0;
}

/*---------------------------------------------------*/

static void canon_out()

{
    CBLOCK  *cbp = in_canon.last;
    CBLOCK  *nbp;

    if ( cbp == (CBLOCK *)0 )
        return;

    --(cbp->in);
    --in_canon.count;

    if ( cbp->out == cbp->in )
    {
        if ( in_canon.first != cbp )
        {
            for ( nbp = in_canon.first; nbp->next != cbp; nbp = nbp->next )
                ;

            nbp->next     = (CBLOCK *)0;
            in_canon.last = nbp;
        }
        else
            in_canon.first = in_canon.last = (CBLOCK *)0;

        cblk_free(cbp);
    }
}

/*---------------------------------------------------*/

static CBLOCK *cblk_alloc()

{
    int     i;
    CBLOCK  *cbp;

    for ( i = 0, cbp = blocks; i < MAX_BLOCKS; ++i, ++cbp )
        if ( cbp->out == CBLK_SIZE )
            break;

    return ( i < MAX_BLOCKS ) ? cbp : (CBLOCK *)0;
}

/*---------------------------------------------------*/

static void cblk_free(cbp)

CBLOCK  *cbp;

{
    cbp->out = CBLK_SIZE;
}

/*---------------------------------------------------*/

static void setraw()

{
    struct termio  tbuf;

    if ( ioctl(STDIN, TCGETA, &tbuf) == -1 ) 
        return;

    tbufsave = tbuf;

    tbuf.c_iflag &= ~(INLCR | ICRNL | IUCLC | ISTRIP | IXON | BRKINT);
    tbuf.c_oflag &= ~OPOST;
    tbuf.c_lflag &= ~(ICANON | ISIG );
    tbuf.c_cc[4]  = 1;                              /* MIN  */
    tbuf.c_cc[5]  = 1;                              /* TIME */

    ioctl(STDIN, TCSETAF, &tbuf);
}

/*---------------------------------------------------*/

static void unsetraw()

{
    ioctl(STDIN, TCSETAF, &tbufsave);
}

/*---------- For test purposes -----------------------*/

static void print_termio()

{
    struct termio   *tbp = &my_buf; 
 
    printf("c_iflag: %d\r\n", (INT) tbp->c_iflag);
    printf("c_oflag: %d\r\n", (INT) tbp->c_oflag);
    printf("c_cflag: %d\r\n", (INT) tbp->c_cflag);
    printf("c_lflag: %d\r\n", (INT) tbp->c_lflag);
    printf("c_line : %c\r\n", tbp->c_line);
    printf("c_cc   : %d %d %d %d %d %d %d %d\r\n", tbp->c_cc[0], tbp->c_cc[1],
                            tbp->c_cc[2], tbp->c_cc[3], tbp->c_cc[4],
                            tbp->c_cc[5], tbp->c_cc[6], tbp->c_cc[7]);
}

