IDIR =.
CC=g++
CFLAGS=-I$(IDIR) -fopenmp --std=c++11 -fpermissive -O3 

ODIR=obj
LDIR =../lib

LIBS=-lm -lgdal 

_DEPS = node.h stripe.h dem.h utils.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = FillDEM_Zhou-onepass.o dem.o parallel_priorityflooding.o utils.o main.o stripe.o

OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

openmpfill: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
