#include "server.h"

#include <string>
#include <iostream>
#include <ctime>
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

const char* pageHtml = 
"<html><body><H1>hi............</H1>"
"<form action=\"/chat.php\"  method=\"post\">"
"Message: <input type=\"text\" name=\"message\"><input type=\"submit\" value=\"Send\">"
"</form></body></html>";

//----------------------------------------------------------------------
// ClientConnectionInfo
//----------------------------------------------------------------------
struct ClientConnectionInfo {
    std::string userName; 
    struct sockaddr_in addr; 
    int addrLen; 
    int socketFd; 
}; 

//--------------------------------------------------------------
// Name: BuildHttpResponse
// Desc:
//--------------------------------------------------------------
std::string BuildHttpResponse(std::string message) {
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
// Name: BuildHttpResponseSSE
// Desc:
//------------------------------------------------------------------------------------
std::string BuildHttpResponseSSE() {

    std::string message = "<!DOCTYPE html><html><body><h1>Getting server updates</h1>"
        "<div id=\"result\"></div><script>\r\n"
        "if(typeof(EventSource) !== \"undefined\") {\r\n"
            "var source = new EventSource(\"event_src.php\");\r\n"
            "source.onmessage = function(event) {\r\n"
            "document.getElementById(\"result\").innerHTML += event.data + \"<br>\";\r\n};\r\n"
        "} else {\r\n"
            "document.getElementById(\"result\").innerHTML = \"Sorry, your browser does not support server-sent events...\";\r\n"
        "} </script></body></html>";

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
std::string BuildSSEResponse() {

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
    // SOCK_DGRAM? is for UDP
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

    // tell the socket that we want to listen for incomming connections
    if(listen(this->sockfd, 32) == -1) {
        this->status = false; 
        std::cout << "listen() failed.\n"; 

        return; 
    }

    // FD_ZERO(&this->readFds); 
    // FD_SET(this->sockfd, &this->readFds);
    // this->maxSd = this->sockfd; 
    this->fdCount = 1; 
    this->readFds[0].fd = this->sockfd; 
    this->readFds[0].events = POLL_IN; 

    this->clilen = sizeof(this->cli_addr);
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
// Name: Connected
// Desc:
//--------------------------------------------------------------------------------------------------------
bool TCPServer::AcceptIncommingConnection(int socketFd, int numClientSockets) {
    
    bool connectionResult = true; 
    
    // struct sockaddr_in clientAddress; 
    // socklen_t clientAddressLen = sizeof(clientAddress); 
    
    // int newConnectionSocketFd = accept(socketFd, (struct sockaddr*) &clientAddress, &clientAddressLen);  

    // if (newConnectionSocketFd >= 0) 
    // {
    //     FD_SET(newConnectionSocketFd, &this->readFds); 

    //     struct sockaddr_in addr; 
    //     int addrLen; 

    //     if(getpeername(newConnectionSocketFd, (sockaddr*) &addr, (socklen_t*) &addrLen) != -1) {
                
    //         // inet_ntoa() converts host address to IPV$ dotted-decimal notation 
    //         // ntoh() convert network byte order to host byte order
    //         std::cout << "Client " << inet_ntoa(addr.sin_addr) << " - " << ntohs(addr.sin_port) << "\n";
    //     }

    //     // read the socket and print out the message we got
    //     auto message = this->ReadFromSocket(newConnectionSocketFd); 

    //     if (message.length() > 0) {
    //         std::cout << message << "\n";

    //         auto pos = message.find("/event_src.php"); 

    //         if (pos != std::string::npos) {
    //             auto response = BuildSSEResponse();

    //             if (write(newConnectionSocketFd, response.c_str(), response.length()) == -1) {
    //                 connectionResult = false; 
    //             }
    //         } else {

    //             auto response = BuildHttpResponseSSE(); 

    //             if (write(newConnectionSocketFd, response.c_str(), response.length()) == -1) {
    //                 connectionResult = false; 
    //             }
    //         }

    //     }
    // }

    return connectionResult; 
}

//--------------------------------------------------------------------------------------------------------
// Name: GetMessage
// Desc: 
//--------------------------------------------------------------------------------------------------------
bool TCPServer::GetMessage(std::string& message) {

    // copy to working fd set   
    memcpy((void*) &this->workingFds, (void*) &this->readFds, sizeof(this->readFds)); 

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
                        std::cout << "Incomming connection... " << inet_ntoa(addr.sin_addr) << "\n";  

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
std::string TCPServer::ReadFromSocket(int socketFd) {

        char buffer[1024];
        memset(buffer, 0, 1024); 

        // TODO: validate socketFd??

        // data has arrived from an already connected socket
        auto bytesRead = read(socketFd, (void*)buffer, 1024); 

        if (bytesRead > 0) {

            buffer[bytesRead] = 0; 
        }

    return std::string(buffer);
}

//--------------------------------------------------------------------------------------------------------
// Name: ProcessMessage
// Desc:
//--------------------------------------------------------------------------------------------------------
bool TCPServer::ProcessMessage(const char* buffer, unsigned int size, struct pollfd pollFd) {
    
    std::string message(buffer); 

    if (message.find("GET / HTTP/1.1") != std::string::npos) {

        auto response = BuildHttpResponseSSE(); 

        auto result = send(pollFd.fd, response.c_str(), response.length(), 0);

        if (result == -1) {
            std::cout << "send() error\n"; 
        } 

    } else if (message.find("GET /event_src.php HTTP/1.1") != std::string::npos) {

        auto response = BuildSSEResponse(); 

        auto t = std::time(nullptr); 
        auto localTime = *std::localtime(&t); 
        char timeStr[100]; 
        std::strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &localTime);

        response += "\r\nretry: 15000\r\n";     
        response += "data: ";
        response += timeStr; 
        response += "\r\n\r\n";  

        if (send(pollFd.fd, response.c_str(), response.length(), 0) == -1) {
            std::cout << "send() error\n"; 
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