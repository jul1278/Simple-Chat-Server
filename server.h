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

const int MAX_CLIENTS = 100; 

//------------------------------------------------------------------------------------------------------
// Name: PollFdInfo
//------------------------------------------------------------------------------------------------------
struct PollFdInfo {
    struct pollfd pollFds[MAX_CLIENTS]; 
    int usedNumFds; 
};

//------------------------------------------------------------------------------------------------------
// Name: ServerStreamInfo
//------------------------------------------------------------------------------------------------------
struct ServerStreamInfo {
    time_t lastStreamTime; 
    std::unordered_set<int> streamClientFds; 

    //-------------------------------------------------------------------
    // Name: InsertFd
    //-------------------------------------------------------------------    
    ServerStreamInfo() {
        this->lastStreamTime = std::time(nullptr); 
    }

    //-------------------------------------------------------------------
    // Name: InsertFd
    //-------------------------------------------------------------------
    void InsertFd(int fd) {
        if (this->streamClientFds.find(fd) == this->streamClientFds.end()) {
            this->streamClientFds.insert(fd); 
        }
    }
};

//------------------------------------------------------------------------------------------------------
// Name: TCPServer
//------------------------------------------------------------------------------------------------------
class TCPServer {
    
    ServerResources serverResources; 

    bool status = false; 
    int sockfd;
     
    int portno;

    socklen_t clilen;
 
    struct PollFdInfo pollFdInfo; 
    struct ServerStreamInfo serverStreamInfo; 

    sockaddr_in serv_addr;
    void CloseClientConnection(const int index); 
    
    void StreamUpdatesToClients(struct ServerStreamInfo& serverStreamInfo);
    struct PollFdInfo AcceptIncomingConnections(const int listeningSocketId,  const struct PollFdInfo& pollFdInfo); 
    bool ProcessRequest(const char* buffer, unsigned int size, const int pollFd);
    bool ReadSocket(int fd); 

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
