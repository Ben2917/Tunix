    .file   "thread.c"
    .version    "01.01"
    .text
    .globl  init_threads
    .align  4
init_threads:
    jmp .L21
.L20:
    pushl   $main_regs
    call    save_context
    popl    %ecx
    xorl    %edi,%edi
/LOOP   BEG
    jmp .L18
/LOOP   HDR
.L15:
    imull   $4136,%edi,%eax
    movl    $0,threads(%eax)
    imull   $4136,%edi,%eax
    movl    $5555,threads+40(%eax)
    incl    %edi
.L18:
/LOOP   COND
    cmpl    $16,%edi
    jl  .L15
/LOOP   END
    movl    $16,thread_cnt
.L19:
    popl    %edi
    leave
    ret/0
.L21:
    pushl   %ebp
    movl    %esp,%ebp
    pushl   %edi
    jmp .L20
    .type   init_threads,@function
    .size   init_threads,.-init_threads
    .globl  start_thread
    .align  4
start_thread:
    jmp .L31
.L30:
    cmpl    $0,thread_cnt
    jg  .L22
    jmp .L29
.L22:
    movl    $threads,%edi
/LOOP   BEG
    jmp .L26
/LOOP   HDR
.L23:
    cmpl    $0,(%edi)
    jne .L27
    jmp .L25
.L27:
    leal    4136(%edi),%edi
.L26:
/LOOP   COND
    cmpl    $threads+66176,%edi
    jnae    .L23
/LOOP   END
.L25:
    cmpl    $threads+66176,%edi
    jnae    .L28
    .section    .data1,"aw"
    .align  4
.X32:

    .byte   0x74,0x68,0x72,0x65,0x61,0x64,0x73,0x20,0x61,0x6c
    .byte   0x6c,0x20,0x75,0x73,0x65,0x64,0x20,0x75,0x70,0x00
    .text
    pushl   $.X32
    call    panic
    popl    %ecx
    jmp .L29
.L28:
    movl    %edi,%eax
    subl    $threads,%eax
    movl    $4136,%ecx
    cltd
    idivl   %ecx
    movl    %eax,this_proc
    leal    4132(%edi),%eax
    movl    %eax,-4(%ebp)
    movl    $1,(%edi)
    decl    thread_cnt
    pushl   -4(%ebp)
    call    new_context
    popl    %ecx
/REGAL  0   AUTO    -4(%ebp)    4
.L29:
    popl    %edi
    leave
    ret/0
.L31:
    pushl   %ebp
    movl    %esp,%ebp
    pushl   %eax
    pushl   %edi
    jmp .L30
    .type   start_thread,@function
    .size   start_thread,.-start_thread
    .globl  end_thread
    .align  4
end_thread:
    jmp .L43
.L42:
    movl    this_proc,%edi
    incl    thread_cnt
    imull   $4136,this_proc,%eax
    movl    $0,threads(%eax)
    imull   $4136,this_proc,%eax
    cmpl    $5555,threads+40(%eax)
    je  .L33
    .section    .data1
    .align  4
.X44:

    .byte   0x73,0x74,0x61,0x63,0x6b,0x20,0x6f,0x76,0x65,0x72
    .byte   0x66,0x6c,0x6f,0x77,0x20,0x69,0x6e,0x20,0x74,0x68
    .byte   0x72,0x65,0x61,0x64,0x00
    .text
    pushl   $.X44
    call    panic
    popl    %ecx
.L33:
/LOOP   BEG
/LOOP   HDR
.L34:
    imull   $4136,this_proc,%eax
    cmpl    $1,threads(%eax)
    jne .L37
    jmp .L36
.L37:
    movl    this_proc,%eax
    incl    %eax
    movl    $16,%ecx
    cltd
    idivl   %ecx
    movl    %edx,this_proc
    cmpl    %edi,this_proc
    jne .L38
    jmp .L36
.L38:
/LOOP   COND
    jmp .L34
/LOOP   END
.L36:
    cmpl    %edi,this_proc
    jne .L39
    movl    $main_regs,-4(%ebp)
    jmp .L40
.L39:
    imull   $4136,this_proc,%eax
    leal    threads+8(%eax),%eax
    movl    %eax,-4(%ebp)
.L40:
    pushl   -4(%ebp)
    call    restore_context
    popl    %ecx
/REGAL  0   AUTO    -4(%ebp)    4
.L41:
    popl    %edi
    leave
    ret/0
.L43:
    pushl   %ebp
    movl    %esp,%ebp
    pushl   %eax
    pushl   %edi
    jmp .L42
    .type   end_thread,@function
    .size   end_thread,.-end_thread
    .globl  sleep
    .align  4
sleep:
    jmp .L55
.L54:
    movl    this_proc,%edi
    imull   $4136,%edi,%eax
    leal    threads+8(%eax),%eax
    pushl   %eax
    call    save_context
    popl    %ecx
    imull   $4136,this_proc,%eax
    cmpl    $5555,threads+40(%eax)
    je  .L45
    .section    .data1
    .align  4
.X56:

    .byte   0x73,0x74,0x61,0x63,0x6b,0x20,0x6f,0x76,0x65,0x72
    .byte   0x66,0x6c,0x6f,0x77,0x20,0x69,0x6e,0x20,0x74,0x68
    .byte   0x72,0x65,0x61,0x64,0x00
    .text
    pushl   $.X56
    call    panic
    popl    %ecx
.L45:
    imull   $4136,this_proc,%eax
    movl    $2,threads(%eax)
    imull   $4136,this_proc,%eax
    movl    8(%ebp),%edx
    movl    %edx,threads+4(%eax)
/LOOP   BEG
/LOOP   HDR
.L46:
    imull   $4136,this_proc,%eax
    cmpl    $1,threads(%eax)
    jne .L49
    jmp .L48
.L49:
    movl    this_proc,%eax
    incl    %eax
    movl    $16,%ecx
    cltd
    idivl   %ecx
    movl    %edx,this_proc
    cmpl    %edi,this_proc
    jne .L50
    jmp .L48
.L50:
/LOOP   COND
    jmp .L46
/LOOP   END
.L48:
    cmpl    %edi,this_proc
    jne .L51
    movl    $main_regs,-4(%ebp)
    jmp .L52
.L51:
    imull   $4136,this_proc,%eax
    leal    threads+8(%eax),%eax
    movl    %eax,-4(%ebp)
.L52:
    pushl   -4(%ebp)
    call    restore_context
    popl    %ecx
/REGAL  0   AUTO    -4(%ebp)    4
/REGAL  0   PARAM   8(%ebp) 4
.L53:
    popl    %edi
    leave
    ret/0
.L55:
    pushl   %ebp
    movl    %esp,%ebp
    pushl   %eax
    pushl   %edi
    jmp .L54
    .type   sleep,@function
    .size   sleep,.-sleep
    .globl  wakeup
    .align  4
wakeup:
    jmp .L64
.L63:
    movl    8(%ebp),%esi
    movl    $threads,%edi
/LOOP   BEG
    jmp .L60
/LOOP   HDR
.L57:
    cmpl    $2,(%edi)
    jne .L61
    cmpl    %esi,4(%edi)
    jne .L61
    movl    $1,(%edi)
.L61:
    leal    4136(%edi),%edi
.L60:
/LOOP   COND
    cmpl    $threads+66176,%edi
    jnae    .L57
/LOOP   END
.L62:
    popl    %esi
    popl    %edi
    leave
    ret/0
.L64:
    pushl   %ebp
    movl    %esp,%ebp
    pushl   %edi
    pushl   %esi
    jmp .L63
    .type   wakeup,@function
    .size   wakeup,.-wakeup
    .ident  "acomp: (SCDE) 5.0  09/24/90"
    .data
    .local  main_regs
    .comm   main_regs,32,4
    .local  this_proc
    .comm   this_proc,4,4
/REGAL  0   STATEXT this_proc   4
    .local  threads
    .comm   threads,66176,4
    .comm   thread_cnt,4,4
/REGAL  0   EXTDEF  thread_cnt  4
