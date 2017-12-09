#include "Server.h"
#include "ServerResources.h"

#include <string>
#include <iostream>
#include <ctime>
#include <unordered_map>

#include <iomanip>

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

    this->lastStreamTime = std::time(nullptr); 

    this->fdCount = 1; 
    this->readFds[0].fd = this->sockfd; 
    this->readFds[0].events = POLL_IN; 

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
// Name: GetMessage
// Desc: 
//--------------------------------------------------------------------------------------------------------
bool TCPServer::GetMessage(std::string& message) {

    // stream updates to clients first
    auto t = std::time(nullptr); 
    auto currentTime = *std::localtime(&t); 
    auto delta = difftime(t, this->lastStreamTime); 

    if (delta >= 1.0) {
        for (auto fd : this->streamClientFds) {

            auto response = GetSSEResponse(); 

            char timeStr[100]; 
            std::strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &currentTime);

            response += "\r\nretry: 15000\r\n";     
            response += "data: ";
            response += timeStr; 
            response += "\r\n\r\n";  

            if (send(fd, response.c_str(), response.length(), 0) == -1) {
                std::cout << "send() error\n"; 
            }
        }

        this->lastStreamTime = t; 
    }

    auto activity = poll(this->readFds, MAX_CLIENTS, 0); 

    if (activity <= 0) {
        // error or timeout
        return false; 
    }

    for (auto i = 0; i < this->fdCount; i++) {

        if (this->readFds[i].revents == 0 || this->readFds[i].revents != POLL_IN) {
            // nothing happen
            continue; 
        }

        // incomming connections on listening socket
        if (this->readFds[i].fd == this->sockfd) {
            
            int newConnectionSocketFd = 0;

            // listening socket is readable
            while(newConnectionSocketFd != -1) {
                struct sockaddr_in clientAddress; 
                socklen_t clientAddressLen = sizeof(clientAddress); 

                // accept new incoming connection
                newConnectionSocketFd = accept(this->sockfd, (struct sockaddr*) &clientAddress, &clientAddressLen); 

                if (newConnectionSocketFd < 0) {
                    if (errno != EWOULDBLOCK) { /* error! */ }
                    break;

                } else {
                    if (this->fdCount < MAX_CLIENTS) {
                            
                        this->readFds[this->fdCount].fd = newConnectionSocketFd; 
                        this->readFds[this->fdCount].events = POLL_IN; 

                        this->fdCount++;

                        struct sockaddr_in addr; 
                        socklen_t len = sizeof(addr); 

                        auto result = getpeername(newConnectionSocketFd, (struct sockaddr*) &addr, &len); 
                        std::cout << "Incoming connection... " << inet_ntoa(addr.sin_addr) << "\n";  

                    } else {

                        std::cout << "Can't accept any more connections\n";
                        break; 
                    }
                }
            } 
        } else {
            // descriptor is readable 

            while(true) {

                char buffer[1024]; 

                // TODO: what is the last argument
                auto result = recv(this->readFds[i].fd, (void*)buffer, sizeof(buffer), 0); 

                if (result < 0) {
                    if (errno != EWOULDBLOCK) {
                        std::cout << "recv() error\n"; 

                        // TODO: close this connection
                    }

                    break; 

                } else if (result == 0) {

                    // TODO: close this connection
                    break;

                } else {

                    buffer[result] = 0; 
                    std::cout << buffer << "\n";

                    this->ProcessMessage(buffer, result, this->readFds[i]); 
                }
            }
        }

    }

    return true; 
}

//--------------------------------------------------------------------------------------------------------
// Name: ProcessMessage
// Desc:
//--------------------------------------------------------------------------------------------------------
bool TCPServer::ProcessMessage(const char* buffer, unsigned int size, struct pollfd pollFd) {
    
    std::string message(buffer); 

    if (message.find("GET / HTTP/1.1") != std::string::npos) {

        auto response = BuildHttpResponseSSE(this->serverResources); 

        auto result = send(pollFd.fd, response.c_str(), response.length(), 0);

        if (result == -1) {
            std::cout << "send() error\n"; 
        } 

    } else if (message.find("GET /event_src.php HTTP/1.1") != std::string::npos) {

        if (this->streamClientFds.find(pollFd.fd) == this->streamClientFds.end()) {
            this->streamClientFds.insert(pollFd.fd); 
        }
    }

    return true; 
}

//--------------------------------------------------------------------------------------------------------
// Name: Connected
// Desc:
//--------------------------------------------------------------------------------------------------------
bool TCPServer::Connected() {
    return this->status; 
}