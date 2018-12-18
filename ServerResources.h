// ServerResources.h
#ifndef SERVER_RESOURCES_H
#define SERVER_RESOURCES_H

#include <string>
#include <fstream>
#include <unordered_map>

//----------------------------------------------------------------------
// ServerResources
//----------------------------------------------------------------------
class ServerResources {

    std::unordered_map<std::string, std::string> namedResource; 

public:

    ServerResources() {

    }

    //----------------------------------------------------------------------
    // Name: LoadResource
    // Desc:
    //----------------------------------------------------------------------
    void LoadResource(const std::string& file) {
        
        // TODO: validate file exists etc
        
        std::ifstream in(file);
        std::string contents((std::istreambuf_iterator<char>(in)), 
        std::istreambuf_iterator<char>());

        namedResource[file] = contents;  
    }

    //----------------------------------------------------------------------
    // Name: GetNamedResource
    // Desc:
    //----------------------------------------------------------------------
    std::string GetNamedResource(const std::string& name) const {
        if (namedResource.find(name) != namedResource.end()) {
            return namedResource.at(name); 
        }

        return std::string(); 
    }
};

#endif // SERVER_RESOURCES_H
