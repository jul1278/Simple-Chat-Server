// client.h
#ifndef CLIENT_H
#define CLIENT_H

#include <string>

//-------------------------------------------------------------------------------------------
// Name:
// Desc:
//-------------------------------------------------------------------------------------------
class TCPClient {

public:

    //-------------------------------------------------------------------------------------------
    // Name:
    // Desc:
    //-------------------------------------------------------------------------------------------
    TCPClient(const std::string& name) {

    }

    //-------------------------------------------------------------------------------------------
    // Name:
    // Desc:
    //-------------------------------------------------------------------------------------------
    ~TCPClient() {

    }

    //-------------------------------------------------------------------------------------------
    // Name:
    // Desc:
    //-------------------------------------------------------------------------------------------
    bool Send(const std::string& message) {
        return false; 
    }

    //-------------------------------------------------------------------------------------------
    // Name:
    // Desc:
    //-------------------------------------------------------------------------------------------
    bool Connected() {
        return false; 
    }
};

#endif // CLIENT_H