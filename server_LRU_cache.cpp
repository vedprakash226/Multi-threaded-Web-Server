#include "proxy_parse.h"
#include<bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define MAX_BYTES 4096    //max allowed size of request/response
#define MAX_CLIENTS 400     //max number of client requests served at a time
#define MAX_SIZE 10*(1<<20)     //size of the cache
#define MAX_ELEMENT_SIZE 1*(1<<20)     //max size of an element in cache

struct cacheElement{
    int length;
    time_t timestamp_cache;
    std::string url;
    std::vector<char> data;
    cacheElement* next;
    cacheElement* prev; 

    cacheElement(): next(nullptr), prev(nullptr){}
};

std::unordered_map<std::string, cacheElement*> cacheMap;  //hash map for quick lookup

int portNumber;          
int proxyServerSocketID;

pthread_t threadID[MAX_CLIENTS];
pthread_mutex_t lock;   // mutex lock to avoid race condition on cache storage shared resource
sem_t semaphore;   // semaphore to limit number of clients

// LRU cache variables head
cacheElement* head;     //most recently used element
cacheElement* tail;     //least recently used element
int cacheSize;

//helper functions
void removeNode(cacheElement* node){
    if(node==nullptr) return;

    if(node==head) head = head->next;
    if(node==tail) tail=tail->prev;
    if(node->prev) node->prev->next = node->next;
    if(node->next) node->next->prev = node->prev;

    node->next = nullptr;
    node->prev = nullptr;

    return;
}

void addToFront(cacheElement* node){
    if(node==nullptr or node==head) return;

    node->next = head;
    node->prev = nullptr;

    if(head) head->prev = node;
    head = node;
    if(tail == nullptr) tail = head;

    return;
}


// LRU cache functions
cacheElement* find(std::string &url){
    pthread_mutex_lock(&lock);  //locking the shared resource
    std::cout<<"Locked the cache for lookup"<<std::endl;

    auto it = cacheMap.find(url);
    if(it==cacheMap.end()){
        pthread_mutex_unlock(&lock);    //unlocking the shared resource
        std::cout<<"Not found in cache."<<std::endl;
        return nullptr;   //not found in cache
    }

    
    cacheElement* node = it->second;
    removeNode(node);
    addToFront(node);

    node->timestamp_cache = time(nullptr);

    pthread_mutex_unlock(&lock);    //unlocking the shared resource
    std::cout<<"Unlocked the cache after lookup"<<std::endl;
    return node;
}

// remove least recently used element from cache
void remove_cacheElement(){
    if(tail==nullptr) return;

    cacheElement* lru = tail;
    removeNode(lru);
    cacheMap.erase(lru->url);

    if(tail==lru){
        tail = lru->prev;
    }
    if(head==nullptr) tail = nullptr;

    cacheSize-= (lru->length + sizeof(cacheElement)+ lru->url.size());
    delete lru;
}

// function to add element to cache
int add_cacheElement(std::vector<char> &data, std::string& url){
    //aquire the lock
    pthread_mutex_lock(&lock);
    std::cout<<"Locked the cache for addition"<<std::endl;

    //find the size of the new element
    int sizeNewElement = data.size() + sizeof(cacheElement) + url.size();

    if(sizeNewElement>MAX_ELEMENT_SIZE){
        pthread_mutex_unlock(&lock);
        std::cerr<<"Element size exceeds maximum allowed size. Not caching this element."<<std::endl;
        return 0;
    }

    //check if max cahe size exceeded
    while(cacheSize + sizeNewElement > MAX_SIZE){
        remove_cacheElement();
    }

    //create new cache element
    cacheElement* newElement = new cacheElement;
    newElement->length = data.size();
    newElement->timestamp_cache = time(nullptr);
    newElement->data = data;
    newElement->url = url;
    addToFront(newElement);
    cacheMap[url] = newElement;

    cacheSize+=sizeNewElement;

    //release the lock
    pthread_mutex_unlock(&lock);
    return 1;
}

// check if the HTTP version is supported
int checkHTTPversion(char *msg){
	if(!strncmp(msg, "HTTP/1.1", 8)) return 1;
	else if(!strncmp(msg, "HTTP/1.0", 8)) return 1;
	
    return -1;
}

//sending error message function
int sendErrorMessage(int socket, int status_code)
{
	char str[1024];
	char currentTime[50];
	time_t now = time(0);

	struct tm data = *gmtime(&now);
	strftime(currentTime,sizeof(currentTime),"%a, %d %b %Y %H:%M:%S %Z", &data);

	switch(status_code)
	{
		case 400: snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
				  printf("400 Bad Request\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 403: snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
				  printf("403 Forbidden\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 404: snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
				  printf("404 Not Found\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 500: snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
				  //printf("500 Internal Server Error\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 501: snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
				  printf("501 Not Implemented\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 505: snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
				  printf("505 HTTP Version Not Supported\n");
				  send(socket, str, strlen(str), 0);
				  break;

		default:  return -1;

	}
	return 1;
}

int connectRemoteServer(char* hostAddress, size_t port_num){
    int remoteSocketId = socket(AF_INET, SOCK_STREAM, 0);
    if(remoteSocketId<0){
        std::cerr<<"Remote server socket creation failed"<<std::endl;
        return -1;
    }

    //resolve the host address
    hostent* serverHost = gethostbyname(hostAddress);
    if(serverHost==nullptr){
        std::cerr<<"Failed to resolve host: "<<hostAddress<<std::endl;
        return -1;
    }

    // filling the sercer address structure
    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port= htons(port_num);
    std::memcpy(&serverAddress.sin_addr.s_addr, serverHost->h_addr, serverHost->h_length);

    if(connect(remoteSocketId, (sockaddr*)&serverAddress, sizeof(serverAddress))<0){
        std::cerr<<"Connection to remote server failed"<<std::endl;
        return -1;
    }

    return remoteSocketId;
}

// client handler
int handleRequest(int clientSocketID, ParsedRequest* request, std::string &tempReq){
    std::vector<char> buffer(MAX_BYTES, '\0');
    strcpy(buffer.data(), "GET ");
    strcat(buffer.data(), request->path.c_str());
    strcat(buffer.data(), " ");
    strcat(buffer.data(), request->version.c_str());
    strcat(buffer.data(), "\r\n");

    size_t bufferLength = strlen(buffer.data());

    request->setHeader("Connection", "close");

    if(request->port.empty()){
        request->setHeader("Host", request->host);
    } else {
        request->setHeader("Host", request->host + ":" + request->port);
    }

    if(request->unparse_headers(buffer.data()+bufferLength, (size_t)(MAX_BYTES-bufferLength))<0){
        std::cerr<<"Failed to unparse headers"<<std::endl;
    }

    size_t serverPort = 80;
    if(!request->port.empty()){
        serverPort = stoi(request->port);
    }

    int remoteSocketID = connectRemoteServer((char*)request->host.c_str(), serverPort);
    if(remoteSocketID<0) return -1;

    int bytesSent = send(remoteSocketID, buffer.data(), strlen(buffer.data()), 0);
    fill(buffer.begin(), buffer.end(), '\0');

    bytesSent= recv(remoteSocketID, buffer.data(), MAX_BYTES-1, 0);

    //temporary buffer to store response
    std::vector<char> tempResponse;
    tempResponse.reserve(MAX_BYTES);

    while(bytesSent>0){
        //send to client
        send(clientSocketID, buffer.data(), bytesSent, 0);

        //store in temporary response buffer
        for(int i{0};i<bytesSent;i++){
            tempResponse.push_back(buffer[i]);
        }

        if(bytesSent<0){
            std::cerr<<"Error in receiving data from remote server"<<std::endl;
            break;
        }

        std::fill(buffer.begin(), buffer.end(), '\0');
        bytesSent= recv(remoteSocketID, buffer.data(), MAX_BYTES-1, 0);
    }

    add_cacheElement(tempResponse, tempReq);
    close(remoteSocketID);
    return 0;
}

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
        dataLength = strlen(buffer.data());

        //find if the request ended?
        if(strstr(buffer.data(), "\r\n\r\n")==nullptr){
            bytesReceived = recv(socketID, buffer.data()+dataLength, MAX_BYTES - dataLength, 0);
        }else{
            break;
        }
    }

    //make a copy of the buffer to find it in cache
    std::string tempReq(buffer.data(), bytesReceived);

    cacheElement* temp = find(tempReq);      //lookup in the cache if it exits
    if(temp!=nullptr){
        // found the data in the cache
        std::cout<<"Data found in cache. Served from cache."<<std::endl;

        int dataSize = temp->length;
        int pos = 0;
        char response[MAX_BYTES];

        while(pos<dataSize){
            std::memset(response, '\0', MAX_BYTES);
            for(int i{0}; i<MAX_BYTES and pos<dataSize; i++){
                response[i] = temp->data[pos++];
            }
            send(socketID, response, MAX_BYTES, 0);
        }

    }
    else if(bytesReceived>0){
        //data not found in cache
        //parse the reques
        ParsedRequest *request = new ParsedRequest();

        if(request->parse(buffer.data(), dataLength)<0){
            std::cerr<<"Request parsing failed"<<std::endl;
        }else{
            std::fill(buffer.begin(), buffer.end(), '\0');

            if(request->method == "GET"){
                if(!request->host.empty() and !request->path.empty() and checkHTTPversion((char*)request->version.c_str())==1){
                    bytesReceived = handleRequest(socketID, request, tempReq);

                    // if no response from the server
                    if(bytesReceived==-1){
                        sendErrorMessage(socketID, 500);
                    }
                }else{
                    sendErrorMessage(socketID, 500);
                }
            }else{
                std::cerr<<"Unsupported HTTP method received: "<<request->method<<std::endl;
            }
        }
        
        delete request;
    }
    else if(bytesReceived==0){
        std::cerr<<"Client disconnected unexpectedly."<<std::endl;
    }
    else{
        std::cerr<<"Error in receiving data from client."<<std::endl;
    }

    shutdown(socketID, SHUT_RDWR);
    close(socketID);
    sem_post(&semaphore);    //it increases the value of semaphore by 1
    sem_getvalue(&semaphore, &currentSemaphoreVal);
    std::cout<<"Connection closed. Current semaphore value: "<<currentSemaphoreVal<<std::endl;

    return nullptr;

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

    //initialize cache size
    cacheSize = 0;
    head = nullptr;
    tail = nullptr;

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