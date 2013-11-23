CFLAGS := -lreadline -lm -std=gnu99 -g -Wall 

all: clean shell.bin

sfs.o: sfs.c sfs.h
	gcc $(CFLAGS) -c sfs.c

shell.o: shell.c sfs.h
	gcc $(CFLAGS) -c shell.c

shell.bin: sfs.o shell.o
	gcc $(CFLAGS) sfs.o shell.o -o shell.bin
clean:
	rm -f *.o *.bin
