
#--> flags for x86-64 using default work loop increment (to get 10
#--> minute runtime on a 3.0GHz processor use -w 14 -n 280000).  Note
#--> that with gcc V4.3 and -g generates memory traffic in the work
#--> loop. This is bad because it memory reference latency produces
#--> a lot of noise.  Changing the optimization level to -O1 or above
#--> eliminates the work loop entirely.  This is even worse.  It is
#--> recommended to use assembly language below if you have an x86
#--> compatible processor. If not, write your own!  
#CFLAGS =-I../common -O1 -march=native -mtune=native -m64 -static flags

CC    ?= gcc
CXX   ?= g++
MPICC ?= mpicc

ARCH = $(shell uname -m)

ifneq (,$(findstring x86_64,$(ARCH)))
#--> for x86-64 using assembly code work (use -w 19 -n 500000)
CFLAGS =  -DASMx8664 -O1 -fexpensive-optimizations -march=native -mtune=native -m64 -malign-double
#-static

#--> flags for x86-64 with vectorization using daxpy work (use -w 14 -n 500000)
#CFLAGS =  -I../common -DDAXPY -O3 -ffast-math -funroll-loops -fexpensive-optimizations -march=native -mtune=native -msse4.2 -m64 -malign-double -static -ftree-vectorizer-verbose=3

#--> flags for x86-64 without vectorization using daxpy work (use -w 14 -n 500000)
#CFLAGS =  -I../common -DDAXPY -O1 -ffast-math -funroll-loops -fexpensive-optimizations -march=native -mtune=native -msse4.2 -m64 -malign-double -static

#--> flags for BGP
#CFLAGS =-qasm=gcc  -I../common -O0 -qunroll -DBGP

else ifneq (,$(findstring ppc64,$(ARCH)))
CFLAGS = -DDAXPY -O3 -ffast-math -funroll-loops -fexpensive-optimizations -mcpu=native -mtune=native -m64 -ftree-vectorizer-verbose=3

else
CFLAGS = -g -O0
endif 


all: fwq fwq-th fwq-mpi

single: fwq

thread: fwq-th

mpi: fwq-mpi

# Fixed WORK quanta benchmark without threads
fwq: ftq.h fwq.c
	$(CC) $(CFLAGS) fwq.c -o fwq

# Fixed WORK quanta benchmark without threads assembly language
# output. This is most useful to view and verify the loop you think
# you are running is the loop the cores/threads are actually
# executing.
fwq.s: ftq.h fwq.c
	$(CC) $(CFLAGS) -S fwq.c

# Fixed WORK quanta benchmark for use with mutiple threads
fwq-th: ftq.h fwq-th.c cpu.o
	$(CC) -I../utils $(CFLAGS) -g -Wall fwq-th.c cpu.o -o fwq-th -lpthread

# Use MPI processes to run an FWQ instance per core
fwq-mpi: ftq.h fwq-mpi.c cpu.o
	$(MPICC) -I../utils $(CFLAGS) -g -Wall fwq-mpi.c cpu.o -o fwq-mpi

cpu.o: ../utils/cpu.c
	$(CC) -Wall -Werror -c $< 

clean:
	rm -f *~ *.o fwq fwq-th fwq-mpi
