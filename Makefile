CC?=cc
LIBM?=-lm
RM?=rm -f
WFLAGS?=-Wall
CFLAGS+=$(WFLAGS)

.PHONY: all
all: txtelite

txtelite: txtelite.o
	$(CC) -o $@ $< $(LDFLAGS) $(LIBM)

txtelite.o: txtelite.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	-$(RM) txtelite txtelite.exe txtelite.o core *.core
