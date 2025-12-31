CC=g++
CFLAGS= -g -Wall

all: proxy

proxy: server_LRU_cache.cpp
	$(CC) $(CFLAGS) -o proxy_parse.o -c proxy_parse.c -lpthread
	$(CC) $(CFLAGS) -o proxy.o -c server_LRU_cache.cpp -lpthread
	$(CC) $(CFLAGS) -o proxy proxy_parse.o proxy.o -lpthread

clean:
	rm -f *.o proxy