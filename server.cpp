#include "server.h"

#include <string>
#include <iostream>
#include <ctime>
#include <iomanip>

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
        "<div id=\"result\"></div><script>"
        "if(typeof(EventSource) !== \"undefined\") {"
            "var source = new EventSource(\"demo_sse.php\");"
            "source.onmessage = function(event) {"
            "document.getElementById(\"result\").innerHTML += event.data + \"<br>\";"
        "} else {"
            "document.getElementById(\"result\").innerHTML = \"Sorry, your browser does not support server-sent events...\";"
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
    response += "Content-Type: text/event-stream\r\n";
    response += "Cache-Control: no-cache\r\n\r\n";

    auto t = std::time(nullptr); 
    auto localTime = *std::localtime(&t); 
    char timeStr[100]; 
    std::strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H-%M-%S", &localTime);
    
    response += "date: ";
    response += timeStr; 
    response += "\r\n";  

    return response; 
}

//------------------------------------------------------------------------------------
// Name: TCPServer
// Desc:
//------------------------------------------------------------------------------------
TCPServer::TCPServer(const std::string& name, const unsigned int port) {
    
    memset(this->clientSockets, 0, MAX_CLIENTS*sizeof(int)); 
    char buffer[256];
    unsigned int n;

    // create a new socket
    // SOCK_STREAM indicates that we want a TCP socket
    // SOCK_DGRAM? is for UDP
    this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (this->sockfd < 0) {
        this->status = false; 
        std::cout << "socket() failed.\n"; 
        return; 
    }

    memset((char *) &this->serv_addr, 0, sizeof(this->serv_addr));
    
    auto result = -1;
    auto maxNumPorts = 100; 
    auto counter = 0; 
    
    // keep trying to bind to ports until we find one
    while (result < 0)  {

        // INADDR_ANY socket accepts connections to any of the machines IPs
        this->serv_addr.sin_family = AF_INET;
        this->serv_addr.sin_addr.s_addr = INADDR_ANY;
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
    if(listen(this->sockfd, 10) < 0) {
        this->status = false; 
        std::cout << "listen() failed.\n"; 

        return; 
    }

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
    
    struct sockaddr_in clientAddress; 
    socklen_t clientAddressLen = sizeof(clientAddress); 
    auto newConnectionSocketFd = accept(socketFd, (struct sockaddr*) &clientAddress, &clientAddressLen);

    if (newConnectionSocketFd >= 0) 
    {
        // Add connection
        for (auto i = 0; i < MAX_CLIENTS; i++) {
            if (this->clientSockets[i] == 0) {
                this->clientSockets[i] = newConnectionSocketFd; 
                break; 
            }
        }

        auto response = BuildHttpResponse(pageHtml); 

        if (write(newConnectionSocketFd, response.c_str(), response.length()) == -1) {
            connectionResult = false; 
        }
    } else {
        connectionResult = false; 
    }

    return connectionResult; 
}

//--------------------------------------------------------------------------------------------------------
// Name: GetMessage
// Desc: 
//--------------------------------------------------------------------------------------------------------
bool TCPServer::GetMessage(std::string& message) {

    FD_ZERO(&this->readFds);
    FD_SET(this->sockfd, &this->readFds);

    this->maxSd = this->sockfd; 

    for(auto i = 0; i < MAX_CLIENTS; i++) {
        if (this->clientSockets[i] > 0) {
            FD_SET(this->clientSockets[i], &this->readFds); 
        }

        if (this->clientSockets[i] > this->maxSd) {
            this->maxSd = this->clientSockets[i]; 
        }
    }

    auto activity = select(this->maxSd + 1, &this->readFds, nullptr, nullptr, nullptr); 

    if (activity < 0) {
        return false; 
    }

    for (auto i = 0; i < MAX_CLIENTS; i++) {

        if (FD_ISSET(i, &this->readFds)) {

            // this is an incoming connection
            if (i == this->sockfd) {
                auto result = this->AcceptIncommingConnection(this->sockfd, MAX_CLIENTS);
            
            } else {

                char buffer[1024];
                memset(buffer, 0, 1024); 

                // data has arrived from an already connected socket
                auto bytesRead = read(i, (void*)buffer, 1024); 

                if (bytesRead > 0) {

                    buffer[bytesRead] = 0; 

                    this->ProcessMessage(i, buffer, bytesRead); 

                    // print what we recieved 
                    std::cout << buffer << "\n";  

                } else if (bytesRead == 0 && this->clientSockets[i] != 0) {

                    struct sockaddr_in addr; 
                    int addrLen; 

                    if(getpeername(this->clientSockets[i], (sockaddr*) &addr, (socklen_t*) &addrLen) != -1) {
                            
                        // inet_ntoa() converts host address to IPV$ dotted-decimal notation 
                        // ntoh() convert network byte order to host byte order
                        std::cout << "Client " << inet_ntoa(addr.sin_addr) << " - " << ntohs(addr.sin_port) << " disconnected.\n";

                        close(this->clientSockets[i]); 
                        this->clientSockets[i] = 0; 
                    }
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
bool TCPServer::ProcessMessage(int socketIndex, const char* buffer, unsigned int bytesRead) {
    
    // split by \r\n things

    // process get/post separately 

    // should have a ConnectionInfo or something struct rather than just 'socket index'
    
    return true; 
}

//--------------------------------------------------------------------------------------------------------
// Name: Connected
// Desc:
//--------------------------------------------------------------------------------------------------------
bool TCPServer::Connected() {
    return this->status; 
}