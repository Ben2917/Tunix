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

struct xexec
{
    SHORT   x_magic;
    SHORT   x_ext;
    long    x_text;
    long    x_data;
    long    x_bss;
    long    x_syms;
    long    x_reloc;
    long    x_entry;
    char    x_cpu;
    char    x_relsym;
    SHORT   x_renv;
};

struct xext
{
    long    xe_trsize;
    long    xe_drsize;
    long    xe_tbase;
    long    xe_dbase;
    long    xe_stksize;
    long    xe_segpos;
    long    xe_segsize;
    long    xe_mdtpos;
    long    xe_mdtsize;
    char    xe_mdttype;
    char    xe_pagesize;
    char    xe_ostype;
    char    xe_osvers;
    SHORT   xe_eseg;
    SHORT   xe_sres;
};

struct xseg
{
    SHORT   xs_type;
    SHORT   xs_attr;
    SHORT   xs_seg;
    char    xs_align;
    char    xs_cres;
    long    xs_filpos;
    long    xs_psize;
    long    xs_vsize;
    long    xs_rbase;
    SHORT   xs_noff;
    SHORT   xs_sres;
    long    xs_lres;
};
