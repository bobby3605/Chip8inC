CC = /usr/bin/gcc

CFLAGS = -lm -static `sdl2-config --cflags --libs`

emulator: emulator.c
	${CC} ${CFLAGS} $< -o $@
clean:
	rm -rf emulator
