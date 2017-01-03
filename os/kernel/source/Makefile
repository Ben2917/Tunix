OBJS=kernel.o gate.o task.o memory.o kmsg.o kutil.o

.c.o:
	cc -c -O $*.c

../../result/kernel: $(OBJS)
	cc -s -o ../../result/kernel $(OBJS)

../../result/user: user.o stubs.o comm.o
	cc -s -o ../../result/user user.o stubs.o comm.o

../../result/encore: encore.o
	cc -s -o ../../result/encore encore.o

../../result/tree: tree.o stubs.o comm.o
	cc -s -o ../../result/tree tree.o stubs.o comm.o

all: ../../result/kernel ../../result/user ../../result/encore ../../result/tree

