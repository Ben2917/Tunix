OBJS=mm.o pregion.o region.o frame.o fmcall.o swap.o map.o memmap.o\
devcall.o comm.o kcall.o chan.o thread.o context.o 

.c.o:
	cc -c -O $*.c

.s.o:
	cc -c $*.s

../../result/mm: $(OBJS)
	cc -s -o ../../result/mm $(OBJS)

all: ../../result/mm

