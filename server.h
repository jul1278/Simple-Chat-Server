#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <iostream>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <sys/time.h> 
#include <fcntl.h>
#include <errno.h>
#include <list>

const int MAX_CLIENTS = 30; 

enum HttpResponseType {
    GET,
    POST
};

struct HttpResponse {
    HttpResponseType responseType;   
}; 

// TCPServer
class TCPServer {
    
    bool status = false; 
    int sockfd;
    int newsockfd; 
    int portno;

    socklen_t clilen;

    struct pollfd readFds[MAX_CLIENTS]; 
    struct pollfd workingFds[MAX_CLIENTS]; 

    sockaddr_in serv_addr;
    sockaddr_in cli_addr;

    int fdCount;
    
    void CloseConnection(int socketFd); 
    bool AcceptIncommingConnection(int socketFd, int numClientSockets);
    bool ProcessMessage(const char* buffer, unsigned int size, struct pollfd pollFd);
    std::string ReadFromSocket(int socketFd); 

public:
    
    //------------------------------------------------------------------------------------
    // Name: TCPServer
    // Desc:
    //------------------------------------------------------------------------------------
    TCPServer(const std::string& name, const unsigned int port);
    
    ~TCPServer();
    
    //--------------------------------------------------------------------------------------------------------
    // Name: GetMessage
    // Desc: 
    //--------------------------------------------------------------------------------------------------------
    bool GetMessage(std::string& message);

    //--------------------------------------------------------------------------------------------------------
    // Name: Connected
    // Desc:
    //--------------------------------------------------------------------------------------------------------
    bool Connected();
};

#endif // SERVER_H
