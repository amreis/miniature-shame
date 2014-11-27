CC=gcc
LIB_DIR=../lib
INC_DIR=../include
CFLAGS=-m32 -g -Wall -lt2fs
.SUFFIXES:
.SUFFIXES: .o .c

SRCS=t1.c t2.c t3.c
OBJS={SRCS:.c=.o}
EXES=t1 t2 t3 

all: $(EXES)

.c.o:
	$(CC) -L$(LIB_DIR) -I$(INC_DIR) $(CFLAGS) -c $<

t1: t1.o
	$(CC) -o t1 t1.o -I$(INC_DIR) -L$(LIB_DIR) -lt2fs -Wall -m32
t2: t2.o
	$(CC) -o t2 t2.o -I$(INC_DIR) -L$(LIB_DIR) -lt2fs -Wall -m32
t3: t3.o
	$(CC) -o t3 t3.o -I$(INC_DIR) -L$(LIB_DIR) -lt2fs -Wall -m32
clean:
	rm -f *.o
