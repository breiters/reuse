#CFLAGS=-g -O0
UNAME := $(shell uname -s)
ifeq ($(UNAME), Linux)
	CC=gcc
endif
ifeq ($(UNAME), Darwin)
	CC=gcc-7
endif

#CFLAGS=-O3 -fopenmp -march=native
CFLAGS=-g -fopenmp -march=native

all: distgen

clean:
	rm -f *.o distgen *~

install: distgen
	cp distgen ~/bin
