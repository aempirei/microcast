CC = gcc
CCFLAGS = -Wall -W -w
CFLAGS = -Wall -W -I. -pedantic -std=gnu99
LIBFLAGS = -lm
PROGRAMS = throttle
INDENTFLAGS = -i4 -br -ce -nprs -nbfda -npcs -ncs -sob -brf -nut -bap -bad -npsl -l140

.PHONY: all clean wipe tidy

all: $(PROGRAMS)

throttle: throttle.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LIBFLAGS)

clean:
	rm -f *.o *~

wipe: clean
	rm -f $(PROGRAMS)

tidy:
	indent $(INDENTFLAGS) *.c
