#include "proxy_parse.h"
#include <bits/stdc++.h>
#include<semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

#define MAX_CLIENTS 20          //one client per thread

struct cacheElement{
    char* data;
    int length;
    char* url;
    time_t timestamp_cache;
    cacheElement* next;
};

cacheElement* find(char* url);
int add_cacheElement(char* url, char* data, int length);
void remove_cacheElement();

int portNumber = 8080;          //default port number
int proxyServerSocketID;

pthread_t thread_id[MAX_CLIENTS];
pthread_mutex_t lock;   // mutex lock to avoid race condition on cache storage shared resource
sem_t semaphore;   // semaphore to limit number of clients

// LRU cache variables head
cacheElement* head;
int cacheSize;

int main(int argc, char* argv[]){
    int clientSocketID, clientLength;
    struct sockaddr client_addr, server_addr;

    //initialization of semaphore
    sem_init(&semaphore, 0, MAX_CLIENTS);

    //initialization of mutex lock
    pthread_mutex_init(&lock, nullptr);
    
}