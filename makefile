all: main.o
	gcc main.o -lncursesw -o shooting

main.o: main.c
	gcc -c main.c

.PHONY clean:
clean:
	rm -f *.o shooting