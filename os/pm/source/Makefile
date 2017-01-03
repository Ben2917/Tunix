OBJS=pm.o proc.o callout.o fmcall.o mmcall.o ipcmcall.o comm.o\
kcall.o chan.o thread.o context.o

.c.o:
	cc -c -O $*.c

.s.o:
	cc -c $*.s

../../result/pm: $(OBJS)
	cc -s -o ../../result/pm $(OBJS)

all: ../../result/pm

