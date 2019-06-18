CC=gcc
CFLAGS=-O2 -g
ptest: ptest.o
	gcc -O2 -g $< -o $@

clean:
	rm -fr ptest *.o core
