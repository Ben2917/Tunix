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
* This module is only needed in the simulated version of
* the operating system. It won't be needed in the real version.
*
***********************************************************
*/

#include    "../include/ktypes.h"
#include    "../include/fcntl.h"
#include    "../include/kmsg.h"
#include    "../include/gate.h"
#include    "../include/macros.h"
#include    "../include/fcodes.h"
#include    "../include/pid.h"

extern int  open();
extern int  close();
extern void gate_display_state();

extern int  crnt_queue;

int msg_k;
int msg_u;
int msg_i;

int data_k;
int data_u;

int fdl         = -1;
int debug       = FALSE;
int quit        = FALSE; 
int int_active  = FALSE;  
 
static MSG  msg;   
static IMSG imsg;  
static int  verbose     = FALSE; 
 
MSG     *user_msg = &msg;    
    
/*---------------------------------------------------------------*/

main(argc, argv)

int  argc;
char **argv;

{
    while ( argc > 1 && argv[1][0] == '-' )
    {
        switch ( argv[1][1] )
        {
        case 'd':
            debug = TRUE;
            fdl   = open("klog", O_WRONLY | O_CREAT | O_TRUNC, 0600);
            break;
        case 'v':
            verbose = TRUE;
            break;
        }

        --argc;
        ++argv;
    }

    comm_init();
    gate_init(); 
 
    while ( !quit )
    {
        while ( int_active )
        {
            int_active = FALSE;

            rcv_interrupt(&imsg);
            gate_handle_interrupt(imsg.int_no, imsg.arg);
        }

        if ( quit )
            break;

        if ( verbose && (crnt_queue >= INIT_PID && crnt_queue < MAX_PID) ) 
            gate_display_state(); 
 
        if ( receive(crnt_queue) == -1 )
            continue;

        if ( user_msg->queue == 0 )
        {
            switch ( user_msg->cmd )
            {
            case INIT:
                register_pid(user_msg);
                break;
            case LISTEN:
                gate_receive(user_msg);
                break;
            case READ_IN:
                gate_data_out(user_msg);
                break;
            case WRITE_OUT:
                gate_data_in(user_msg);
                break;
            default:
                gate_request(user_msg);
                break;
            }
        }
        else
            gate_send(user_msg);
    }

    comm_end();

    if ( debug )
        (void) close(fdl);

    exit(0);
}


