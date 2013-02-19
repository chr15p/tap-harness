CC=gcc
CFLAGS=-Wall -g 
LIBS=-lm
EXE=harness 
OBJ=harness.o


all: harness

harness: $(OBJ)
	$(CC) $(OBJ) -o $(EXE) $(CFLAGS) $(LIBS)


.c.o:
	$(CC) $(CFLAGS) -c $< 

clean:
	-rm $(OBJ)
	-rm $(EXE)
