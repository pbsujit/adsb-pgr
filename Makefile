# make 'mds02' 'mds02pr'
# mbr20110827

CC=g++
#CFLAG=-g

all:		mds02 mds02pr

mds02:		mds02.o shmsub02.o decsub02.o decbds.o
		$(CC) $(CFGAG) mds02.o shmsub02.o decsub02.o decbds.o \
		-lm -o mds02
		chmod +x ./lookup

mds02.o:	mds02.h mds02.c decsub02.h
		$(CC) $(CFLAG) -c mds02.c -o mds02.o

shmsub02.o:	shmsub02.c mds02.h
		$(CC) $(CFLAG) -c shmsub02.c -o shmsub02.o

decsub02.o:	decsub02.c decsub02.h
		$(CC) $(CFLAG) -c decsub02.c -o decsub02.o

decbds.o:	decbds.c decbds.h mds02.h
		$(CC) $(CFLAG) -c decbds.c -o decbds.o

mds02pr:	mds02pr.o shmsub02.o
		$(CC) $(CFGAG) mds02pr.o shmsub02.o -lm -o mds02pr

mdspr02.o:	mds02pr.cc mds02.h
		$(CC) $(CFLAG) -c mds02pr.cc -o mds02pr.o

clean:
	rm -f *.o mds02 mds02pr
