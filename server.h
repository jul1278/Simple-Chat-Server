#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <iostream>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
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
    fd_set readFds; 

    sockaddr_in serv_addr;
    sockaddr_in cli_addr;

    std::string chatLog;  

    int maxSd = 0; 

    int clientSockets[MAX_CLIENTS];

    bool AcceptIncommingConnection(int socketFd, int numClientSockets);
    bool ProcessMessage(int socketIndex, const char* buffer, unsigned int bytesRead);

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
