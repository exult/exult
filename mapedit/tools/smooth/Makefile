CC=gcc

all: smooth libsmooth_randomize.so libsmooth_stream.so libsmooth_smooth.so

smooth: config.c config.h globals.h image.c image.h linked.c linked.h param.c param.h plugin.c plugin.h smooth.c smooth.h
	${CC} -o smooth smooth.c config.c image.c linked.c param.c plugin.c      `pkg-config --libs --cflags sdl3 sdl3-image` -lm -Wall -Wextra

libsmooth_randomize.so: plugins/plugin_randomize.c
	${CC} -o libsmooth_randomize.so plugins/plugin_randomize.c -fPIC -shared `pkg-config --libs --cflags sdl3 sdl3-image` -lm -Wall -Wextra

libsmooth_stream.so: plugins/plugin_stream.c
	${CC} -o libsmooth_stream.so plugins/plugin_stream.c -fPIC -shared       `pkg-config --libs --cflags sdl3 sdl3-image` -lm -Wall -Wextra

libsmooth_smooth.so: plugins/plugin_smooth.c
	${CC} -o libsmooth_smooth.so plugins/plugin_smooth.c -fPIC -shared       `pkg-config --libs --cflags sdl3 sdl3-image` -lm -Wall -Wextra

tests: smooth libsmooth_randomize.so libsmooth_stream.so libsmooth_smooth.so rough.bmp
	LD_LIBRARY_PATH=. ./smooth -d 4

clean:
	rm -f smooth libsmooth_randomize.so libsmooth_stream.so libsmooth_smooth.so
