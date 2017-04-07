CFLAGS=-Wall -g
CXXFLAGS=-Wall -g -std=c++11
LDFLAGS=-g
LDLIBS=-lstdc++
SERVEROBJS=server.o funcs.o
CLIENTOBJS=client.o funcs.o

all: server client

server: $(SERVEROBJS)

client: $(CLIENTOBJS)

clean:
	rm -f *.o server client