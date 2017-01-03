/*
/***********************************************************
/*
/* Module  : %M%
/* Release : %I%
/* Date    : %E%
/*
/* Copyright (c) 1989 R. M. Switzer
/* 
/* Permission to use, copy, or distribute this software for
/* private or educational purposes is hereby granted without
/* fee, provided that the above copyright notice and this
/* permission notice appear in all copies of the software.
/* The software may under no circumstances be sold or employed
/* for commercial purposes.
/*
/* THIS SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF
/* ANY KIND, EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT
/* LIMITATION ANY WARRANTY OF FITNESS FOR A PARTICULAR PURPOSE. 
/*
/***********************************************************
/*
/* This module is architecture dependent.
/*
/************************************************************

    .file   "context.s"
    .version    "02.01"
/-----------------------------------------------------------------
    .data
    .text
    .globl  new_context
    .align  4 

new_context:                /        Stack looks as follows:
    pushl   %ecx            /
    pushl   %esi            /   +-------------------------+
    pushl   %edi            /   | retadr from new_context |<- sp
    movl    16(%esp) , %edi /   +-------------------------+
    movl    (%ebp), %esi    /   | arg. for new_context    |
    movl    %esi , %ecx     /   +-------------------------+
    subl    %ebp , %ecx     /   |  locals of start_thread |
    shrl    $2 , %ecx       /   |         ...             |
                            /   +-------------------------+
.L1:                        /   |      bp in main         |<-bp
    movl    (%esi) , %eax   /   +-------------------------+
    movl    %eax , (%edi)   /   | retadr from start_thread|
    subl    $4, %esi        /   +-------------------------+
    subl    $4, %edi        /   |   locals of main        |
    loop    .L1             /   |         ...             |
                            /   +-------------------------+ 
    movl    16(%esp) , %ebp /   |   bp in runtime code    |
    movl    %edi , %eax     /   +-------------------------+
    addl    $4, %eax        /   |    retadr from main     |
    popl    %edi            /   +-------------------------+
    popl    %esi            /   |       ......            |
    popl    %ecx
    movl    %eax, %esp
    ret
    .type   new_context,@function
    .size   new_context,.-new_context
/---------------------------------------------------------------- 
    .globl  save_context
    .align  4 

save_context:               / Stack looks as follows:
                            / +------------------------------+
    pushl   %ebx            / | retadr from save_context     |<- sp 
    movl    8(%esp) , %ebx  / +------------------------------+ 
    movl    %ebp , %eax     / | regs (arg. for save_context) |          
    addl    $4 , %eax       / +------------------------------+
    movl    %eax , (%ebx)   / |   locals of init_threads     |
    movl    (%ebp) , %eax   / |        ..........            |
    movl    %eax , 4(%ebx)  / +------------------------------+
    movl    %esi , 8(%ebx)  / |          edi                 |
    movl    %edi , 12(%ebx) / +------------------------------+   
    movl    %ecx , 20(%ebx) / |       bp from main           |<- bp
    movl    %edx , 24(%ebx) / +------------------------------+
    movl    (%esp) , %eax   / |   retadr from init_threads   |
    movl    %eax , 16(%ebx) / +------------------------------+ 
    movl    4(%ebp) , %eax  / |     locals of main           |
    movl    %eax , 28(%ebx) / |      ..............          |
    popl    %ebx            / +------------------------------+
    ret                     / |      bp from runtime         |
                            / +------------------------------+
                            / |      retadr from main        |
                            / +------------------------------+
                            / |         ........             |
    .type   save_context,@function
    .size   save_context,.-save_context
/----------------------------------------------------------------
    .globl  restore_context
    .align  4 

restore_context:
    movl    4(%esp) , %ebx
    movl    24(%ebx) , %edx
    movl    20(%ebx) , %ecx
    movl    12(%ebx) , %edi
    movl    8(%ebx) , %esi 
    movl    4(%ebx) , %ebp
    movl    (%ebx) , %esp
    movl    28(%ebx), %eax
    movl    16(%ebx), %ebx
    movl    %eax, (%esp)
    ret
    .type   restore_context,@function
    .size   restore_context,.-restore_context

