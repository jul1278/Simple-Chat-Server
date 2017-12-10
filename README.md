# Simple-Chat-Server
Simple HTTP chat server in C++ 

Just using BSD sockets, no libraries or anything because I want to learn the low level stuff. 
Currently just streaming (SSE) the time to connected clients updating every second. 

Once I have a basic proof of concept working I intend to maybe implement reliable multicasting (if it's not too difficult) but so far just using regular old TCP

clone the repo and run:

```
g++ -c *.cpp -g -std=c++11
g++ *.o -o server
./server 
```

hardcoded to start on port 5004 or incrementally higher if unavailable 

