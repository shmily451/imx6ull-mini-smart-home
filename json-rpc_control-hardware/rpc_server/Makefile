# 不定义 CC，使用环境变量
INCDIR = -I/home/book/Desktop/json-rpc_control-hardware/libev_pc/tmp/include -I/home/book/Desktop/json-rpc_control-hardware/jsonrpc-c_pc/tmp/include
LIBDIR = -L/home/book/Desktop/json-rpc_control-hardware/libev_pc/tmp/lib -L/home/book/Desktop/json-rpc_control-hardware/jsonrpc-c_pc/tmp/lib
LIBS = -Wl,-Bstatic -ljsonrpcc -lev -Wl,-Bdynamic -lm -pthread

OBJS = rpc_server.o led.o dht11.o cJSON.o

all: rpc_server

rpc_server: $(OBJS)
	$(CC) -o $@ $^ $(LIBDIR) $(LIBS)

%.o: %.c
	$(CC) $(INCDIR) -c -o $@ $<

clean:
	rm -f rpc_server *.o
