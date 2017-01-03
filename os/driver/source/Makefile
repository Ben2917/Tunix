OBJS=driver.o comm.o kcall.o thread.o context.o

.c.o:
	cc -O -c $*.c

.s.o:
	cc -c $*.s

../../result/devhd: devhd.o $(OBJS)
	cc -s -o ../../result/devhd devhd.o $(OBJS)

../../result/devsw: devsw.o $(OBJS)
	cc -s -o ../../result/devsw devsw.o $(OBJS)

../../result/devtty: devtty.o $(OBJS)
	cc -s -o ../../result/devtty devtty.o $(OBJS)

all: ../../result/devhd ../../result/devsw ../../result/devtty

