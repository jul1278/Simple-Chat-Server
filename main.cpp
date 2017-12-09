#include <iostream>

#include "Server.h"

#include <string>
#include <list>

//-------------------------------------------------------------------------------------------
// Name: main
// Desc:
//-------------------------------------------------------------------------------------------
int main(int argc, char** argv) {

    TCPServer server("Server", 5004); 
    
    if (server.Connected()) {
            
        std::cout << "server started\n"; 
        
        while(server.Connected()) {
            std::string message; 
            server.GetMessage(message); 
        }
    }
   
    return 0; 
}