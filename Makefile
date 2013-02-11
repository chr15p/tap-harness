CC=gcc
CFLAGS=-Wall -g 
EXE=harness 
OBJ=harness.o


all: harness

harness: $(OBJ)
	$(CC) $(OBJ) -o $(EXE) $(CFLAGS)


.c.o:
	$(CC) $(CFLAGS) -c $< 

clean:
	-rm $(OBJ)
	-rm $(EXE)
