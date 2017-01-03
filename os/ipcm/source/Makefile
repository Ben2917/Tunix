OBJS=ipcm.o msg.o sem.o shm.o map.o id.o proc.o\
mmcall.o kcall.o chan.o comm.o thread.o context.o

.c.o:
	cc -c -O $*.c

.s.o:
	cc -c $*.s

../../result/ipcm: $(OBJS)
	cc -s -o ../../result/ipcm $(OBJS)

all: ../../result/ipcm

