CC?=cc
LIBM?=-lm
RM?=rm -f
WFLAGS?=-Wall
CFLAGS+=$(WFLAGS)

.PHONY: all
all: txtelite

txtelite: txtelite.o
	$(CC) -o $@ $< $(LDFLAGS) $(LIBM)

.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	-$(RM) txtelite *.o core *.core *.exe *.EXE
