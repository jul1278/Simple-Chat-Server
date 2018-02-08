#include <iostream>

#include "Server.h"

#include <string>
#include <list>

//-------------------------------------------------------------------------------------------
// Name: main
// Desc:
//-------------------------------------------------------------------------------------------
int main(int argc, char** argv) {

    // dont want to get signal crash we can handle errors     
    signal(SIGPIPE, SIG_IGN);

    TCPServer server("Server", 5004); 
    
    if (server.Connected()) {
            
        std::cout << "server started\n"; 
        
        while(server.Connected()) {
            std::string message; 
            server.Update(message); 
        }
    }
   
    return 0; 
}