#ifndef SERVER_H
#define SERVER_H

#include "ServerResources.h"

#include <string>
#include <iostream>

#include <stdio.h>
#include <unordered_set>
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

// TCPServer
class TCPServer {
    
    ServerResources serverResources; 

    bool status = false; 
    int sockfd;
     
    int portno;

    socklen_t clilen;
    time_t lastStreamTime;  

    struct pollfd readFds[MAX_CLIENTS]; 
    std::unordered_set<int> streamClientFds; 

    sockaddr_in serv_addr;

    int fdCount;

    void CloseClientConnection(int index); 

    bool ProcessRequest(const char* buffer, unsigned int size, struct pollfd pollFd);

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
