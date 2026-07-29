CC = gcc
INCDIR = -I/home/book/Desktop/json-rpc/libev_pc/tmp/include \
         -I/home/book/Desktop/json-rpc/jsonrpc-c_pc/tmp/include
LIBDIR = -L/home/book/Desktop/json-rpc/libev_pc/tmp/lib \
         -L/home/book/Desktop/json-rpc/jsonrpc-c_pc/tmp/lib
LIBS = -ljsonrpcc -lev

OBJS = rpc_client.o cJSON.o

all: rpc_client

rpc_client: $(OBJS)
	$(CC) -o $@ $^ $(LIBDIR) $(LIBS)

%.o: %.c
	$(CC) $(INCDIR) -c -o $@ $<

clean:
	rm -f rpc_client *.o
