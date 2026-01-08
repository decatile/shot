compile: miniaudio.o sound.o main.c
	gcc -o shot main.c miniaudio.o sound.o -lm -Wall -Wextra -Wpedantic

miniaudio.o: miniaudio.h
	gcc -x c -c -o miniaudio.o miniaudio.h -DMINIAUDIO_IMPLEMENTATION

sound.o: sound.mp3
	ld -r -b binary -o sound.o sound.mp3

install: compile
	cp shot ~/.local/bin/shot

clean:
	rm -f .* shot
