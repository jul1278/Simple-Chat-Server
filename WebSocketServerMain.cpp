#include <string>
#include <iostream>

#include <queue>
#include <unordered_map> 
#include <set>

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
#include <assert.h>
#include <signal.h>
#include <algorithm>
#include <cstring>
#include <netdb.h>

#include <openssl/sha.h>
#include <NibbleAndAHalf/base64.h>

#include "ServerResources.h"

const int MAX_CLIENTS = 100; 
const int INVALID_FD = -1; 

const char* wsKey = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

unsigned int port = 5004; 

volatile int forceShutdownFlag = 0; 

std::string serverHost = "localhost";  

std::set<int> wsSockets; 

// UserInfo
struct UserInfo {

    UserInfo() 
    {}

    UserInfo(const std::string& name, std::string color, int fd)
    : name(name), color(color), fd(fd) 
    {}

    std::string name;
    std::string color;
    int fd; 
};

std::unordered_map<int, UserInfo> fdUserInfoMap; 

enum ResponseType {
    GET, 
    POST
};

// TODO: this is technically either an HttpRequest or HttpResponse 
// should really be called HttpMessage I think 
struct HttpResponse {
    ResponseType responseType; // in a request this is called the 'method' 
    std::string uri;  
    std::unordered_map<std::string, std::string> headerMap; 
};

//------------------------------------------------------------------------------------------------------
// Name: PollFdInfo
//------------------------------------------------------------------------------------------------------
struct PollFdInfo {

    std::queue<int> invalidFdIndexes; 
    struct pollfd pollFds[MAX_CLIENTS]; 

    int largestUsedIndex;  

public:

    //------------------------------------------------------------------------------------------
    // Name: PollFdInfo
    //------------------------------------------------------------------------------------------
    PollFdInfo(int listeningSocket) {

        memset(this->pollFds, 0, sizeof(this->pollFds));
        this->pollFds[0].fd = listeningSocket; 
        this->pollFds[0].events = POLLIN;

        this->largestUsedIndex = 0; 
    }

    //------------------------------------------------------------------------------------------
    // Name: ~PollFdInfo
    //------------------------------------------------------------------------------------------
    ~PollFdInfo() {
        this->CloseAll(0); 
    }

    //------------------------------------------------------------------------------------------
    // Name: CloseAll
    //------------------------------------------------------------------------------------------
    void CloseAll(int signal) {
        for (auto i = 0; i < this->largestUsedIndex; i++) {
            if (this->pollFds[i].fd > 0) {
                close(this->pollFds[i].fd); 
            }
        }
    }

    //------------------------------------------------------------------------------------------
    // Name: AddFd
    //------------------------------------------------------------------------------------------
    void AddFd(int newFd) {

        auto index = this->largestUsedIndex; 

        if (!this->invalidFdIndexes.empty()) {
            index = this->invalidFdIndexes.front(); 
            this->invalidFdIndexes.pop(); 

            assert(index < MAX_CLIENTS); 
            this->pollFds[index].fd = newFd; 

            if (index > this->largestUsedIndex) {
                this->largestUsedIndex = index; 
            }
            
            return;
        } 

        this->pollFds[index + 1].fd = newFd; 
        this->pollFds[index + 1].events = POLLIN; // DONT forget to tell the socket we want to poll it
        this->largestUsedIndex++; 
    }

    //------------------------------------------------------------------------------------------
    // Name: RemoveFd
    //------------------------------------------------------------------------------------------
    void RemoveFd(int removeFd) {

        auto lastUsedIndex = 0; 
        
        for (auto i = 0; i <= this->largestUsedIndex; i++) {
            
            if (this->pollFds[i].fd == removeFd) {
                this->invalidFdIndexes.push(i); 
                this->pollFds[i].fd = INVALID_FD;
                this->pollFds[i].revents = 0; 

                // if (i == this->largestUsedIndex) {
                //     // find the next smallest used index 
                //     this->largestUsedIndex = lastUsedIndex;
                // }

                return; 
            }

            if (this->pollFds[i].fd != INVALID_FD) {
                lastUsedIndex = i; 
            }
        }
    }
};

//------------------------------------------------------------------------------------
// Name: EncodePayloadToSend
// Desc:
//------------------------------------------------------------------------------------
void EncodePayload(const uint8_t* message, uint16_t messageLen, uint16_t* frameLen, uint8_t* buffer, uint16_t bufLen) {
    
    // TODO: check buffer is large enough for frame
    
    memset((void*) buffer, 0, bufLen); 
    
    buffer[0] = 1 << 7; // FIN for one frame

    // rsv1, rsv2, rsv3 must be zero pretty much always
    
    buffer[0] |= 1; // opcode is 1 for text

    //buffer[1] = (1 << 7); // fuck it mask on
    
    // payload length
    if (messageLen < 126) {
        buffer[1] |= messageLen; 
    } else {
        // TODO: 
    }

    for (auto i = 0; i < messageLen; i++) {
        buffer[2 + i] = message[i]; 
    }

    *frameLen = 2 + messageLen; 
}

//---------------------------------------------------------------------------------------------------------------------
// Name: 
// Desc: 
//---------------------------------------------------------------------------------------------------------------------
void EncodePayload(const std::string& message, uint16_t* frameLen, uint8_t* buffer, uint16_t bufLen) {
    EncodePayload( (const uint8_t*) message.c_str(), message.size(), frameLen, buffer, bufLen); 
}

//--------------------------------------------------------------------------------------------------
// Name: DecodeFrame
// Desc:
//--------------------------------------------------------------------------------------------------
void DecodeFrame(uint8_t buffer[], uint16_t bufferLen, uint16_t payloadBufferLen, uint8_t* payloadBuffer, uint16_t* payloadLen) {
    
    if (bufferLen < 4) {
        return; 
    }
    
    uint8_t maskByteStart = 2; 

    // lets decrypt the message we recieved
    uint8_t fin = (buffer[0] >> 7) & 1;
    uint8_t rsv1 = (buffer[0] >> 6) & 1; 
    uint8_t rsv2 = (buffer[0] >> 5) & 1;  
    uint8_t opcode = buffer[0] & 0xe;
    uint8_t mask = (buffer[1] >> 7) & 1; 
    uint8_t len = buffer[1] & 0x7f; 
    
    uint16_t extendedLen1 = 0; 
    uint64_t extendedLen2 = 0; 

    //std::cout << "fin: " << (int) fin << "\n";
    //std::cout << "rsv1: " << (int) rsv1 << "\n";
    //std::cout << "rsv2: " << (int) rsv2 << "\n";
    //std::cout << "opcode: " << (int) opcode << "\n";  
    //std::cout << "mask: " << (int) mask << "\n"; 
    //std::cout << "len: " << (int) len << "\n"; 

    if (len == 126) {
        // we have to look at the extended payload len
        // bytes 2 and 3

        extendedLen1 = buffer[3];
        extendedLen1 += buffer[2] << 8;
        
        // we use extendedLen instead of len if it is set 
        // dont add them together or something
        //std::cout << "extended len 1: " << (int) extendedLen1 << "\n";

        maskByteStart = 6;  
    }         
    else if (len == 127) {
        // payload length is 32 bits + 16 bits
        // TODO: 
    }          

    uint8_t maskKey[4];

    maskKey[0] = buffer[maskByteStart];
    maskKey[1] = buffer[maskByteStart + 1];
    maskKey[2] = buffer[maskByteStart + 2];
    maskKey[3] = buffer[maskByteStart + 3];  

    // std::cout << "mask key 0: " << (int) maskKey[0] << "\n";
    // std::cout << "mask key 1: " << (int) maskKey[1] << "\n";
    // std::cout << "mask key 2: " << (int) maskKey[2] << "\n";
    // std::cout << "mask key 3: " << (int) maskKey[3] << "\n";

    // now actually decode the message
    uint16_t payloadStart = maskByteStart + 4; 
    *payloadLen =  extendedLen1 ? extendedLen1 : len; 

    if (*payloadLen + 1 > payloadBufferLen) {
        // 
        std::cout << "DecodeFrame() Warning! payload larger than given buffer\n"; 
        return;
    }

    memset((void*) payloadBuffer, 0, payloadBufferLen); 

    for(uint16_t i = 0; i < *payloadLen && i < payloadBufferLen; i++) {
        payloadBuffer[i] = buffer[payloadStart + i] ^ maskKey[i % 4]; 
    }
}

//------------------------------------------------------------------------------------
// Name: AcceptIncomingConnectionsOnSocket
// Desc:
//------------------------------------------------------------------------------------
void AcceptIncomingConnectionsOnSocket(int listeningSocketFd, PollFdInfo& pollFdInfo) {
    
    while(1) {
    
        auto newFd = accept(listeningSocketFd, NULL, NULL);     
        if (newFd < 0) {

            if (errno != EWOULDBLOCK) {
                std::cout << "accept() fail\n"; 
            }

            break; 
        } 

        // insert a new file descriptor
        pollFdInfo.AddFd(newFd); 
    }
}

//-------------------------------------------------------------------------------------------
// Name: TokenizeHttpResponse
// Desc:
//-------------------------------------------------------------------------------------------
void TokenizeHttpResponse(uint8_t* buffer, int bufferSize, std::unordered_map<std::string, std::string>& httpHeaderMap) {

    uint8_t* start = buffer; 
    uint8_t* colon = buffer; 
    uint8_t* end = buffer + bufferSize; 

    bool colonFoundThisHeader = false; 

    while (buffer < end) {
    if (*buffer == ':' && !colonFoundThisHeader) {
            *buffer = '\0';
            colon = buffer + 1;
            colonFoundThisHeader = true; 
        } 

        if (*buffer == '\n') {
            *buffer = '\0'; 
            // remove '\r' possibly from strings 
            if (buffer > start && *(buffer - 1) == '\r') {
                *(buffer - 1) = '\0';
            }
            
            // if colon is larger than start then we've found a colon since the start of this header
            if (colon > start) {

                // get rid of spaces
                while(*colon == ' ' && colon < buffer) {colon++;}  

                // found a colon in the current string
                httpHeaderMap[(char*)start] = (char*)colon; 

                // std::cout << start << " : " << colon << "\n"; 
                
                colonFoundThisHeader = false; 
                start = buffer + 1;

            } else {

                // get rid of spaces
                while(*start == ' ' && start < buffer) {start++;}    
                
                httpHeaderMap[(char*)start] = (char*)start; 
                // std::cout << start << "\n";
                
                colonFoundThisHeader = false;
                start = buffer + 1; 
            }
        }

        buffer++;
    } 
}

//------------------------------------------------------------------------------------
// Name: ParseHttpResponseBuffer
// Desc:
//------------------------------------------------------------------------------------
void ParseHttpMessage(uint8_t* buffer, int bufferLen, struct HttpResponse* httpResponse) {

    auto newBuffer = buffer; 
    std::vector<std::string> firstLine;  

    // increment until we get past the first line
    // copy characters into the route
    do {
        if (firstLine.empty() || *newBuffer == ' ') {
            // dont push the space character on the string
            firstLine.push_back(std::string()); 
        }

        if (*newBuffer != '\r' && *newBuffer != '\n' && *newBuffer != ' ') {
            
            firstLine.back() += *((char*)newBuffer); // append the actual char NOT the pointer!
        }
    } while(*(newBuffer++) != '\n');

    // either we have:
    // GET HTTP/1.1 
    // or
    // GET *URI* HTTP/1.1
    // i think 

    if (firstLine.size() >= 3 && firstLine[2] == "HTTP/1.1") {
        // firstLine[1] is the URI
        httpResponse->uri = firstLine[1]; 
    } 

    if (strncmp("GET", (char*)buffer, 3) == 0) {
        httpResponse->responseType = GET; 
    } 
    else if (strncmp("POST", (char*)buffer, 4) == 0) {
        httpResponse->responseType = POST;
    }
    // BUG: bufferLen needs to be shorter after we modify newBuffer
    //TokenizeHttpResponse(newBuffer, bufferLen, httpResponse->headerMap);
    TokenizeHttpResponse(buffer, bufferLen, httpResponse->headerMap);
}

//------------------------------------------------------------------------------------
// Name: SHAHash
// Desc: routing 
//------------------------------------------------------------------------------------
void SHAHash(const char* in, int inLength, uint8_t** out, int* outLength) {
    
    // NOTE: for hashing blocks
    // https://stackoverflow.com/a/9284475

    SHA_CTX context; 
    if (!SHA1_Init(&context)) {
        return; 
    }

    auto counter = 0; 

    do {

        if (counter + SHA_DIGEST_LENGTH > inLength) {
            
            if (!SHA1_Update(&context, in + counter, inLength - counter)) {
                // error
                return; 
            }

        } else {
            
            if (!SHA1_Update(&context, in + counter, SHA_DIGEST_LENGTH)) {
                // error
                return; 
            }
        }
        
        counter += SHA_DIGEST_LENGTH; 

    } while (counter < inLength);

    if (!SHA1_Final(*out, &context)) {
        // error
        return;
    }
}

//------------------------------------------------------------------------------------
// Name: SortHttpResponse
// Desc: routing 
//------------------------------------------------------------------------------------
void SortHttpResponse(struct HttpResponse& httpResponse, ServerResources& serverResources, struct pollfd& pollFd) {
    
    if (httpResponse.responseType == GET) {

        if (httpResponse.uri == "/") {

            std::cout << "page get " << pollFd.fd << "\n";

            // requesting the main page/websocket connection
            auto content = serverResources.GetNamedResource("ws.html"); 
            std::string httpHeader = "HTTP/1.1 200 OK \r\nContent-Type: text/html\r\n";
            auto contentLength = std::to_string(content.length()); 
            auto fullResponse = httpHeader + "Content-Length: " + contentLength + "\r\n\r\n" + content + "\r\n\r\n";

            auto len = fullResponse.length();

            auto sendResult = send(pollFd.fd, fullResponse.c_str(), len, 0);

            if (sendResult < 0) {
                std::cout << "send() failed!\n"; 
                return; 
            }

            return; 

        } else if (httpResponse.uri == "/chat") {
            // handle websocket handshake

            std::cout << "WS handshake on " << pollFd.fd << "\n"; 

            // get the websocket key
            auto key = httpResponse.headerMap["Sec-WebSocket-Key"]; 
            key += wsKey;

            uint8_t md[SHA_DIGEST_LENGTH]; 
            uint8_t* p = md;
            
            int outLen; 

            SHAHash(key.c_str(), key.length(), &p, &outLen); 

            int encodedLength = 0; 

            // free this!
            auto base64EncodedBuffer = base64(md, SHA_DIGEST_LENGTH, &encodedLength);

            std::string webSocketHandshakeResponse; 
            
            webSocketHandshakeResponse += "HTTP/1.1 101 Switching Protocols\r\n";
            webSocketHandshakeResponse += "Connection: Upgrade\r\n";
            webSocketHandshakeResponse += "Upgrade: websocket\r\n";
            webSocketHandshakeResponse += "Sec-WebSocket-Accept: ";
            webSocketHandshakeResponse += (char*)base64EncodedBuffer;
            webSocketHandshakeResponse += "\r\n\r\n";

            auto sendResult = send(pollFd.fd, webSocketHandshakeResponse.c_str(), webSocketHandshakeResponse.length(), 0);

            // last thing
            free(base64EncodedBuffer); 

            if (sendResult < 0) {
                std::cout << "send() failed!\n"; 
                std::cout << "WS Handshake failed!\n"; 

                return; 
            }

            wsSockets.insert(pollFd.fd); 

            return; 
        } 
    } 

    std::cout << "unspecified request on " << pollFd.fd << "\n";
}

//------------------------------------------------------------------------------------
// Name: ParseDataMessage
// Desc:
//------------------------------------------------------------------------------------
void ParseDataMessage(UserInfo& userInfo, std::string& message, uint8_t* buffer, uint16_t bufferSize) {
    
    if (bufferSize == 0) {
        return; 
    }

    std::cout << "buffer: " << buffer << std::endl; 

    char separatorChar = '\n'; 
    size_t lastSeparatorIndex = 0; 

    auto nameFieldStr = "name:"; 
    auto colorFieldStr = "color:"; 
    auto chatFieldStr = "chat:";

    auto nextSeparatorIndex = lastSeparatorIndex; 

    while (buffer[nextSeparatorIndex] != separatorChar && ++nextSeparatorIndex < bufferSize) {}

    if (nextSeparatorIndex <= bufferSize) {

        if (nextSeparatorIndex > lastSeparatorIndex) {
            if ( strncmp( nameFieldStr, (const char*) &buffer[lastSeparatorIndex], strlen(nameFieldStr)) )  {
                auto offset = lastSeparatorIndex + strlen(nameFieldStr); 
                userInfo.name = std::string(buffer[offset], nextSeparatorIndex - offset); 
            }   
            else if ( strncmp( colorFieldStr, (const char*)  &buffer[lastSeparatorIndex], strlen(colorFieldStr)) )  {
                auto offset = lastSeparatorIndex + strlen(colorFieldStr); 
                userInfo.color = std::string(buffer[offset], nextSeparatorIndex - offset); 
            }
            else if ( strncmp( chatFieldStr, (const char*) &buffer[lastSeparatorIndex], strlen(chatFieldStr)) )  {
                auto offset = lastSeparatorIndex + strlen(chatFieldStr); 
                message = std::string(buffer[offset], nextSeparatorIndex - offset); 
            }
        }
    } else {
        std::cout << "Error parsing datamessage - ParseDataMessage()\n";
        std::cout << "nextSeparatorIndex: " << nextSeparatorIndex << " bufferSize: " << bufferSize << "\n";  
    }   
}

//------------------------------------------------------------------------------------
// Name: ProcessFds
// Desc:
//------------------------------------------------------------------------------------
void ProcessFds(PollFdInfo& pollFdInfo, int listeningFd, ServerResources& serverResources) {
    
    auto processedFds = 0; 
    
    for (auto i = 0; i < MAX_CLIENTS; i++) {

        // revents is "returned events"
        if (pollFdInfo.pollFds[i].revents == 0) { 
            // nothing happened on this socket
            continue; 
        }

        if (pollFdInfo.pollFds[i].revents != POLL_IN) { 
            
            // something happened
            auto revents = pollFdInfo.pollFds[i].revents;

            if (revents & POLLHUP || 
                revents & POLLERR || 
                revents & POLLNVAL) {

                // close socket

                // TODO: what if this is our main listening socket??
                pollFdInfo.RemoveFd(pollFdInfo.pollFds[i].fd); 
            }

            continue; 
        }

        if (pollFdInfo.pollFds[i].fd == listeningFd) {

            // accept incomming connections on the listening socket        
            AcceptIncomingConnectionsOnSocket(listeningFd, pollFdInfo); 

        } else {
            
            // read from this socket
            const int bufferSize = 1024; 
            ssize_t result = 1; 
            uint8_t buffer[bufferSize]; 

            memset(buffer, 0, bufferSize); 
            
            while(result > 0) {

                result = recv(pollFdInfo.pollFds[i].fd, buffer, bufferSize, 0); 

                if (result < 0 ) {
                    if (errno != EWOULDBLOCK) {
                        std::cout << "recv() error!\n"; 
                    }

                } else {                    

                    // is this socket in our ws set?
                    // lets read the frame and decode the message
                    if (wsSockets.find(pollFdInfo.pollFds[i].fd) != wsSockets.end()) {
                        
                        //
                        uint8_t payload[1024]; 
                        uint16_t payloadLen; 
                        DecodeFrame(buffer, bufferSize, 1024, payload, &payloadLen); 

                        // std::cout << "decoded: " << payload << "\n"; 

                        char splitChar = '|';
                        // split by split character
                        UserInfo info; 
                        std::string message; 

                        ParseDataMessage(info, message, payload, payloadLen); 

                        std::cout << message << std::endl; 

                        uint8_t response[512]; 
                        uint16_t frameLen = 0;

                        EncodePayload(payload, payloadLen, &frameLen, response, 512); 

                        for (auto socketFd : wsSockets) {
                            
                            //int sendResult = send(pollFdInfo.pollFds[i].fd, response, frameLen, 0); 
                            int sendResult = send(socketFd, response, frameLen, 0); 

                            if (sendResult < 0) {
                                std::cout << "send() to FD: " << socketFd << " failed!\n"; 
                                wsSockets.erase(socketFd); 
                                return; 
                            }
                        }



                    } else {
                        struct HttpResponse httpResponse;

                        ParseHttpMessage(buffer, result, &httpResponse); 
                        SortHttpResponse(httpResponse, serverResources, pollFdInfo.pollFds[i]); 

                        std::cout << "\n";

                        for (auto i = 0; i < bufferSize; i++) {
                            std::cout << buffer[i]; 
                            //printf("%u ", buffer[i]);
                        }

                        std::cout << "\n"; 
                    }
                }
            }
        }
    }
} 

//-------------------------------------------------------------------------------------------
// Name: HandleSigInt
// Desc:
//-------------------------------------------------------------------------------------------
void HandleSigInt(int signal) {
    forceShutdownFlag = 1; 
}

//-------------------------------------------------------------------------------------------
// Name
// Desc:
//-------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
   
    // dont want to get signal crash 
    // we can handle errors     
    signal(SIGPIPE, SIG_IGN);

    // handle ctrl+c exit
    signal(SIGINT, HandleSigInt);

    sockaddr_in serv_addr;


    // create a new socket
    // SOCK_STREAM indicates that we want a TCP socket
    auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) {
        std::cout << "socket() failed.\n"; 
        return 0; 
    }

    struct PollFdInfo pollFdInfo(sockfd);

    struct ifaddr* ifAddress; 
    int on = 1; 

    // Allow socket descriptor to be reuseable
    if (setsockopt(sockfd, SOL_SOCKET,  SO_REUSEADDR, (void*) &on, sizeof(on)) == -1)
    {
        std::cout << "setsockopt() failed\n";
        close(sockfd);
        return 0; 
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*) &on, sizeof(on)) == -1) {
        std::cout << "SO_KEEPALIVE setsockopt() failed\n";
        close(sockfd); 
        return 0; 
    }

    // Set socket to be nonblocking. 
    // incomming socket connections inherit from this socket so they will also nonblock 
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
    {
        std::cout << "fcntl() failed\n";
        close(sockfd);
        return 0;
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    
    auto result = -1;
    auto maxNumPorts = 100; 
    auto counter = 0; 

    // INADDR_ANY socket accepts connections to any of the machines IPs
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // keep trying to bind to ports until we find one
    while (result < 0)  {
        serv_addr.sin_port = htons(port + counter);

        // bind the socket thus associating out socket with an address
        result = bind(sockfd, (sockaddr*) &serv_addr, sizeof(serv_addr));
        if (result < 0) {
            std::cout << "failed to bind to port: " << (port + counter) << "\n";  
            
            if (counter > maxNumPorts) {
                return 0;
            }

            counter++; 
        } else {

            std::cout << "bound to port: " << (port + counter) << "\n";  
            port = port + counter; 
        }
    }

    // tell the socket that we want to listen for incomming connections
    if(listen(sockfd, 32) == -1) {
        std::cout << "listen() failed.\n"; 
        return 0; 
    }

    // program argument is so far just: -ip
    // so we'll ignore the first arg because that is the command line
    // confirm that argument 2 is just "-ip" then arg 3 is an ip address
    for (auto i = 0; i < argc; i++) {
        std::cout << i << " " << argv[i] << std::endl; 
    }

    if (argc > 2) {
        if ( strncmp(argv[1], "-ip", 3) == 0 ) {
            int count = 0; 
            int i = 0;
            char c; 

            while ( argv[2][i] ) {
                if ( argv[2][i] == '.' ) {
                    count++;
                }

                i++;
            }

            if (count == 3) {
                serverHost.assign(argv[2]); 
                std::cout << "ip: " << serverHost << std::endl; 

            } else {
                std::cout << "Given IP address: " << argv[2] << " is invalid\n"; 
            } 
        } else {
            std::cout << "ip argument not given\n"; 
        }
    } 

        
    ServerResources resources; 
    resources.SetWildcard("%ip%", serverHost);
    resources.LoadResource("ws.html");

    while(forceShutdownFlag == 0) {
        
        // specify timeout of zero so poll() doesnt block 
        // need to add + 1 to largestUsedIndex otherwise when only the listening socket is in the array
        // largestUsedIndex will be zero and poll wont do anything
        auto activity = poll(pollFdInfo.pollFds, pollFdInfo.largestUsedIndex + 1, 0);

        if (activity < 0) {
            // error
            std::cout << "poll() error!\n"; 
            break; 
        }

        // one or more sockets are readable 
        if (activity > 0) {
            ProcessFds(pollFdInfo, sockfd, resources); 
        }
    }

    return 0; 
}


