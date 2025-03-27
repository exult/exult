CC=gcc

all: mockup

mockup: main.c main.h defs.h
	${CC} main.c -o mockup `pkg-config --libs --cflags sdl3 sdl3-image` -Wall -Wextra

tests: mockup map.png mappings.txt
	./mockup map.png mappings.txt

clean:
	rm -f u7map mockup
