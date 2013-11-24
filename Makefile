CFLAGS := -Iinclude -Lobj -lreadline -lm -std=gnu99 -g -Wall 

all: clean shell.bin

obj/sfs.o: src/sfs.c include/sfs.h
	gcc $(CFLAGS) -c src/sfs.c -o obj/sfs.o

obj/shell/core.o: src/shell/core.c include/shell/core.h
	gcc $(CFLAGS) -c src/shell/core.c -o obj/shell/core.o

obj/shell/commands.o: src/shell/commands.c include/shell/commands.h obj/sfs.o
	gcc $(CFLAGS) -c src/shell/commands.c -o obj/shell/commands.o

obj/shell/main.o: obj/shell/commands.o obj/shell/core.o src/shell/main.c
	gcc $(CFLAGS) -c src/shell/main.c -o obj/shell/main.o

shell.bin: obj/sfs.o obj/shell/main.o
	gcc $(CFLAGS) obj/sfs.o obj/shell/core.o obj/shell/commands.o obj/shell/main.o -o shell.bin

clean:
	rm -f obj/*.o
	rm -f obj/shell/*.o
	rm -f *.bin

fs.dat:
	dd if=/dev/zero of=fs.dat bs=512 count=200
