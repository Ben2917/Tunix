OBJS=fm.o fproc.o sys.o dir.o super.o inode.o buffer.o \
devcall.o raw.o kcall.o comm.o chan.o thread.o context.o

.c.o:
	cc -c -O $*.c

.s.o:
	cc -c $*.s

../../result/fm: $(OBJS) 
	cc -s -o ../../result/fm $(OBJS) 
 
../../result/mkfs: mkfs.o
	cc -s -o ../../result/mkfs mkfs.o

../../result/fsck: fsck.o
	cc -s -o ../../result/fsck fsck.o

all : ../../result/fm ../../result/mkfs ../../result/fsck

