CC=gcc
CFLAGS=-Wall -g -rdynamic 
LIBS=-lm -ldl
EXE=harness 
OBJS=harness.o

DYNOBJS=stdout.so 

all: $(EXE) $(DYNOBJS) 

$(DYNOBJS):
	gcc -g -Wall -shared -c -fPIC $(basename $@).c -o $(basename $@).o
	gcc -g -Wall -shared -o $@ $(basename $@).o


harness: $(OBJS) 
	$(CC) $(OBJS) -o $(EXE) $(CFLAGS) $(LIBS)


#.c.o:
%.o: %.c
	$(CC) $(CFLAGS) -c $< 

clean:
	-rm $(OBJS)
	-rm $(EXE)
	-rm $(DYNOBJS)
