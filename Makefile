IDIR =../include
CC=gcc
CFLAGS=-I../include -Iportaudio/include -march=native -Rpass-analysis=loop-vectorize -O3 -DLOG_USE_COLOR

ODIR=obj
LDIR =../lib

LIBS_fixwav=-lm   -lsndfile

_DEPS =
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ_fixwav = fixwav.o log.o
OBJ_fixwav = $(patsubst %,$(ODIR)/%,$(_OBJ_fixwav))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fixwav: $(OBJ_fixwav)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS_fixwav)

all: fixwav

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~  fixwav
