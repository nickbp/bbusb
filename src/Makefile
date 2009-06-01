CC=gcc
CFLAGS=-c --std=c99 -Wall -Wextra -pedantic -g
LDFLAGS=-lusb-1.0

PROG=bbusb
SOURCES=config.c hardware.c main.c packet.c
OBJFILES=$(SOURCES:.c=.o)

$(PROG): $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

all: $(SOURCES) $(PROG)

clean: 
	rm -f $(OBJFILES) $(PROG)
