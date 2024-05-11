CC=gcc

all: mockup

mockup: main.c main.h defs.h
	${CC} main.c -o mockup `pkg-config --libs --cflags sdl2 SDL2_image` -Wall -Wextra

tests: mockup map.png mappings.txt
	./mockup map.png mappings.txt

clean:
	rm -f u7map mockup
