#include "proxy_parse.h"
#include <bits/stdc++.h>
#include <pthread.h>
#include<semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<errno.h>

#define MAX_CLIENTS 20          //one client per thread
#define MAX_BYTES 10240

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

int portNumber;          
int proxyServerSocketID;

pthread_t threadID[MAX_CLIENTS];
pthread_mutex_t lock;   // mutex lock to avoid race condition on cache storage shared resource
sem_t semaphore;   // semaphore to limit number of clients

// LRU cache variables head
cacheElement* head;
int cacheSize;

// Thread function to handle each client request
void* threadFunc(void* newSocket){
    sem_wait(&semaphore);    //it decreases the value of semaphore by 1 and waits until it is less than 0

    int currentSemaphoreVal{};
    sem_getvalue(&semaphore, &currentSemaphoreVal);
    std::cout<<"current semaphore value: "<<currentSemaphoreVal<<std::endl;

    int socketID = *static_cast<int*>(newSocket);

    // now the socket is open and client will send the information
    ssize_t bytesReceived{};
    size_t dataLength{};

    //buffer to store client request at runtime
    std::vector<char> buffer(MAX_BYTES, '\0');

    bytesReceived = recv(socketID, buffer.data(), MAX_BYTES, 0);     //buffer.data() returns char* to the contiguous memory

    while(bytesReceived>0){
        //find how much buffer is used
        dataLength = std::strlen(buffer.data());

        //find if the request ended?
        if(std::strstr(buffer.data(), "\r\n\r\n")==nullptr){
            bytesReceived = recv(socketID, buffer.data()+dataLength, MAX_BYTES - dataLength, 0);
        }else{
            break;
        }
    }

    //make a copy of the buffer to find it in cache
    std::string tempReq(buffer.data(), std::strlen(buffer.data()));

    cacheElement* temp = find(tempReq.data());      //lookup in the cache if it exits
    if(temp!=nullptr){
        int dataSize = temp->length/sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];

        while(pos<dataSize){
            std::memset(response, '\0', MAX_BYTES);
            for(int i{0}; i<MAX_BYTES and pos<dataSize; i++){
                response[i] = temp->data[pos++];
            }
            send(socketID, response, std::strlen(response), 0);
        }

        // found the data in the cache
        std::cout<<"Data found in cache. Served from cache."<<std::endl;
    }


}


int main(int argc, char* argv[]){
    // checking if the port number is provided
    if(argc==2){
        portNumber = atoi(argv[1]);
    }else{
        std::cerr<<"Too few arguments. Provide port number."<<std::endl;
        exit(1);
    }

    std::cout<<"Starting Proxy Server on port "<<portNumber<<std::endl;

    //initialization of semaphore
    sem_init(&semaphore, 0, MAX_CLIENTS);

    //initialization of mutex lock
    pthread_mutex_init(&lock, nullptr);

    //starting the proxy server
    proxyServerSocketID = socket(AF_INET, SOCK_STREAM, 0);

    //if server socket creattion fails
    if(proxyServerSocketID<0){
        std::cerr<<"Socket creation failed...Exiting."<<std::endl;
        exit(1);
    }

    //if server socket created then we have to reuse it
    int reuse = 1;
    if(setsockopt(proxyServerSocketID, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse))<0){
        std::cerr<<"Socket reuse failed...Exiting."<<std::endl;
        exit(1);
    }

    int clientSocketID, clientLength;
    sockaddr_in clientAddress = {}, serverAddress = {};     //initializing server and client address structures

    // filling server address structure
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(portNumber);
    serverAddress.sin_addr.s_addr = INADDR_ANY;         //allocate any address

    //binding the server socket
    if(bind(proxyServerSocketID, (sockaddr*)&serverAddress, sizeof(serverAddress))<0){
        std::cerr<<"Port is not available...Exiting."<<std::endl;
        exit(1);
    }

    std::cout<<"Binding on port "<<portNumber<<" successful."<<std::endl;

    int listenStatus = listen(proxyServerSocketID, MAX_CLIENTS);
    if(listenStatus<0){
        std::cerr<<"Listening failed...Exiting."<<std::endl;
        exit(1);
    }

    std::vector<int> connectedSocketID(MAX_CLIENTS);
    int iterator = 0;

    while(true){
        clientLength = sizeof(clientAddress);

        //accepting client connection
        // clientSocketID = accept(proxyServerSocketID, (sockaddr*) &clientAddress, (socklen_t*)&clientLength);
        clientSocketID = accept(proxyServerSocketID, (sockaddr*) &clientAddress, (socklen_t*)&clientLength);
        if(clientSocketID<0){
            std::cerr<<"Client connection failed..."<<std::endl;
            exit(1);
        }

        connectedSocketID[iterator] = clientSocketID;

        sockaddr_in* client_ptr = &clientAddress;
        in_addr IPaddress = client_ptr->sin_addr;
        char clientIP[INET_ADDRSTRLEN];      // INET_ADDRSTRLEN -> max length of string to hold IPv4 address
        inet_ntop(AF_INET, &IPaddress, clientIP, INET_ADDRSTRLEN);

        std::cout<<"Client is connected from IP: "<<clientIP<<", Port: "<<ntohs(client_ptr->sin_port)<<std::endl;

        pthread_create(&threadID[iterator], nullptr, threadFunc, (void*)&connectedSocketID[iterator]);
        iterator++;

    }   

    close(proxyServerSocketID);
    return 0;

}