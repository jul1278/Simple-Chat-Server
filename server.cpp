#include "Server.h"
#include "ServerResources.h"

#include <string>
#include <iostream>
#include <ctime>
#include <unordered_map>
#include <iomanip>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <sys/time.h> 
#include <fcntl.h>
#include <errno.h>
#include <list>

//------------------------------------------------------------------------------------
// Name: BuildHttpResponseSSE
// Desc:
//------------------------------------------------------------------------------------
std::string BuildHttpResponseSSE(const ServerResources& serverResources) {

    const auto message = serverResources.GetNamedResource("chat.html"); 

    std::string response; 

    auto len = std::to_string(message.length()); 

    response += "HTTP/1.1 200 OK\r\n"; 
    response += "Content-Length: ";
    response += len;
    response += "\r\n"; 
    response += "Content-Type: text/html\n\n";

    response += message; 

    return response;
}

//------------------------------------------------------------------------------------
// Name: BuildHttpResponseUnauthorised
// Desc:
//------------------------------------------------------------------------------------
std::string BuildHttpServiceUnavailable() {
    auto response = "HTTP/1.1 503 Service Unavailable\r\n\r\n";
    return response;
}

//------------------------------------------------------------------------------------
// Name: BuildSSEResponse
// Desc:
//------------------------------------------------------------------------------------
std::string GetSSEResponse() {

    std::string response; 

    response += "HTTP/1.1 200 OK\r\n"; 
    response += "Connection: keep-alive\r\n";
    response += "Content-Type: text/event-stream\r\n";

    return response; 
}

//------------------------------------------------------------------------------------
// Name: TCPServer
// Desc:
//------------------------------------------------------------------------------------
TCPServer::TCPServer(const std::string& name, const unsigned int port) {

    // create a new socket
    // SOCK_STREAM indicates that we want a TCP socket
    this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (this->sockfd < 0) {
        this->status = false; 
        std::cout << "socket() failed.\n"; 
        return; 
    }

    int on = 1; 

    // Allow socket descriptor to be reuseable
    if (setsockopt(this->sockfd, SOL_SOCKET,  SO_REUSEADDR, (void*) &on, sizeof(on)) == -1)
    {
        std::cout << "setsockopt() failed\n";
        close(this->sockfd);
        return; 
    }

    // Set socket to be nonblocking. 
    // incomming socket connections inherit from this socket so they will also nonblock 
    if (fcntl(this->sockfd, F_SETFL, O_NONBLOCK) == -1)
    {
        std::cout << "fcntl() failed\n";
        close(this->sockfd);
        return;
    }

    memset((char *) &this->serv_addr, 0, sizeof(this->serv_addr));
    
    auto result = -1;
    auto maxNumPorts = 100; 
    auto counter = 0; 

    // INADDR_ANY socket accepts connections to any of the machines IPs
    this->serv_addr.sin_family = AF_INET;
    this->serv_addr.sin_addr.s_addr = INADDR_ANY;

    // keep trying to bind to ports until we find one
    while (result < 0)  {
        this->serv_addr.sin_port = htons(port + counter);

        // bind the socket thus associating out socket with an address
        result = bind(this->sockfd, (sockaddr*) &this->serv_addr, sizeof(this->serv_addr));
        if (result < 0) {
            std::cout << "failed to bind to port: " << (port + counter) << "\n";  
            
            if (counter > maxNumPorts) {
                this->status = false;
                return;
            }

            counter++; 
        } else {

            std::cout << "bound to port: " << (port + counter) << "\n";  
        }
    }

    // load up our html resources
    this->serverResources.LoadResource("chat.html"); 

    // tell the socket that we want to listen for incomming connections
    if(listen(this->sockfd, 32) == -1) {
        this->status = false; 
        std::cout << "listen() failed.\n"; 

        return; 
    }

    this->pollFdInfo.usedNumFds = 1; 
    this->pollFdInfo.pollFds[0].fd = this->sockfd; 
    this->pollFdInfo.pollFds[0].events = POLL_IN; 

    this->status = true; 
}

//--------------------------------------------------------------------------------------------------------
// Name: TCPServer
// Desc:
//--------------------------------------------------------------------------------------------------------
TCPServer::~TCPServer() {
    close(this->sockfd); 
}

//--------------------------------------------------------------------------------------------------------
// Name: StreamUpdatesToClients
// Desc:
//--------------------------------------------------------------------------------------------------------
void TCPServer::StreamUpdatesToClients(struct ServerStreamInfo& serverStreamInfo) {

    // stream updates to clients first
    auto t = std::time(nullptr); 
    auto currentTime = *std::localtime(&t); 
    auto delta = difftime(t, serverStreamInfo.lastStreamTime); 

    if (delta >= 1.0) {
        for (auto fd : serverStreamInfo.streamClientFds) {

            auto response = GetSSEResponse(); 

            char timeStr[100]; 
            std::strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &currentTime);

            response += "\r\nretry: 15000\r\n";     
            response += "data: ";
            response += timeStr; 
            response += "\r\n\r\n";  

            errno = 0; 
            if (send(fd, response.c_str(), response.length(), 0) == -1) {
                auto errnoCopy = errno; 
                if (errno != EWOULDBLOCK) {
                    std::cout << "send() errno: " << errnoCopy << "\n"; 
                    std::cout << "Closing connection: " << fd << "\n"; 

                    this->CloseClientConnection(fd); 

                    if (serverStreamInfo.streamClientFds.empty()) {
                        break; 
                    }
                }
            }
        }

        serverStreamInfo.lastStreamTime = t; 
    }
}

//--------------------------------------------------------------------------------------------------------
// Name: GetMessage
// Desc: 
//--------------------------------------------------------------------------------------------------------
bool TCPServer::GetMessage(std::string& message) {

    this->StreamUpdatesToClients(this->serverStreamInfo); 

    auto activity = poll(this->pollFdInfo.pollFds, MAX_CLIENTS, 0); 

    if (activity <= 0) {
        // error or timeout
        return false; 
    }

    for (auto i = 0; i < this->pollFdInfo.usedNumFds; i++) {

        if (this->pollFdInfo.pollFds[i].revents == POLL_ERR) {
            this->CloseClientConnection(this->pollFdInfo.pollFds[i].fd); 
            continue; 
        }
        
        if (this->pollFdInfo.pollFds[i].revents == 0) {
            // nothing happen
            continue; 
        }

        // incomming connections on listening socket
        if (this->pollFdInfo.pollFds[i].fd == this->sockfd) {
            this->pollFdInfo = this->AcceptIncomingConnections(i, this->pollFdInfo);

        } else {

            // descriptor is readable 
            while(true) {
                if (!this->ReadSocket(this->pollFdInfo.pollFds[i].fd) ) {
                    break; 
                }
            }
        }
    }

    return true; 
}

//--------------------------------------------------------------------------------------------------------
// Name: ProcessRequest
// Desc:
//--------------------------------------------------------------------------------------------------------
bool TCPServer::ReadSocket(int fd) {
    char buffer[1024]; 

    // TODO: what is the last argument
    auto result = recv(fd, (void*)buffer, sizeof(buffer), 0); 

    if (result < 0) {
        if (errno != EWOULDBLOCK) {
            std::cout << "recv() error\n"; 

            // Close this connection
            this->CloseClientConnection(fd); 
        }

        return false;  

    } else if (result == 0) {

        // Close this connection
        this->CloseClientConnection(fd); 
        return false;

    } else {

        buffer[result] = 0; 
        std::cout << buffer << "\n";

        this->ProcessRequest(buffer, result, fd); 
    }

    return true; 
}

//--------------------------------------------------------------------------------------------------------
// Name: ProcessRequest
// Desc:
//--------------------------------------------------------------------------------------------------------
struct PollFdInfo TCPServer::AcceptIncomingConnections(const int listeningSocketId,  const struct PollFdInfo& pollFdInfo) {
    
    struct PollFdInfo newPollFdInfo = pollFdInfo; 

    int newConnectionSocketFd = 0;
    auto listeningSocketFd = pollFdInfo.pollFds[listeningSocketId].fd; 

    // listening socket is readable
    while(newConnectionSocketFd != -1) {
        struct sockaddr_in clientAddress; 
        socklen_t clientAddressLen = sizeof(clientAddress); 

        // accept new incoming connection
        newConnectionSocketFd = accept(listeningSocketFd, (struct sockaddr*) &clientAddress, &clientAddressLen); 

        if (newConnectionSocketFd < 0) {
            if (errno != EWOULDBLOCK) { /* error! */ }
            break;

        } else {
            if (newPollFdInfo.usedNumFds < MAX_CLIENTS) {
                    
                newPollFdInfo.pollFds[newPollFdInfo.usedNumFds].fd = newConnectionSocketFd; 
                newPollFdInfo.pollFds[newPollFdInfo.usedNumFds].events = POLL_IN; 

                newPollFdInfo.usedNumFds++; 

                struct sockaddr_in addr; 
                socklen_t len = sizeof(addr); 

                auto result = getpeername(newConnectionSocketFd, (struct sockaddr*) &addr, &len); 
                std::cout << "Incoming connection... " << inet_ntoa(addr.sin_addr) << "\n";  

            } else {
                auto unavailableResponse = BuildHttpServiceUnavailable(); 
                
                if(send(newConnectionSocketFd, unavailableResponse.c_str(), unavailableResponse.length(), 0) == -1) {
                    std::cout << "send() error\n"; 
                }

                std::cout << "Can't accept any more connections\n";
                break; 
            }
        }
    } 

    return newPollFdInfo; 
}

//--------------------------------------------------------------------------------------------------------
// Name: ProcessRequest
// Desc:
//--------------------------------------------------------------------------------------------------------
bool TCPServer::ProcessRequest(const char* buffer, unsigned int size, int fd) {
    
    std::string message(buffer); 

    if (message.find("GET / HTTP/1.1") != std::string::npos) {

        auto response = BuildHttpResponseSSE(this->serverResources); 

        auto result = send(fd, response.c_str(), response.length(), 0);

        if (result == -1) {
            std::cout << "send() error\n"; 
            this->CloseClientConnection(fd);
        } 

    } else if (message.find("GET /event_src.php HTTP/1.1") != std::string::npos) {

        this->serverStreamInfo.InsertFd(fd); 

    }

    return true; 
}

//--------------------------------------------------------------------------------------------------------
// Name: CloseClientConnection
// Desc:
//--------------------------------------------------------------------------------------------------------
void TCPServer::CloseClientConnection(int fd) {

    std::cout << "Closing connection fd: " << fd << "\n"; 

    auto newFdCount = this->pollFdInfo.usedNumFds; 

    // close and compress the list
    for(auto i = 0; i < this->pollFdInfo.usedNumFds; i++) {

        if (i < 0) {
            continue; 
        }

        if (this->pollFdInfo.pollFds[i].fd == fd) {
            // remove from streamClient hashset
            this->serverStreamInfo.streamClientFds.erase(this->pollFdInfo.pollFds[i].fd); 
            
            close(this->pollFdInfo.pollFds[i].fd); 
            this->pollFdInfo.pollFds[i].fd = -1; 
            
            newFdCount--; 
        }

        if (this->pollFdInfo.pollFds[i].fd == -1) {
            this->pollFdInfo.pollFds[i] = this->pollFdInfo.pollFds[i+1]; 

            if ((i + 1) < this->pollFdInfo.usedNumFds) {
                this->pollFdInfo.pollFds[i + 1].fd = -1; 
            }
            
            // we need to move i back so we check the new fd                
            i--; 
        }
    }  

    this->pollFdInfo.usedNumFds = newFdCount;
}

//--------------------------------------------------------------------------------------------------------
// Name: Connected
// Desc:
//--------------------------------------------------------------------------------------------------------
bool TCPServer::Connected() {
    return this->status; 
}