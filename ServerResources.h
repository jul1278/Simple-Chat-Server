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
    std::unordered_map<std::string, std::string> wildcards; 

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

        // const maxWildCardLength = 32; 

        // int lastWildcardStart = 0; 
        // int wildCardCounter = 0;


        // for (auto i = 0; i < contents.length(); i++) {
        //     if (contents[i] == '%') {
                
        //         if (wildCardCounter == 0) {
        //             // starting new wildcard 
        //         } else if (wildCardCounter < maxWildCardLength) {
        //             // closing a wildcard
        //             wildCardCounter = 0; 

        //             std::string wc = contents.substr(lastWildCard, i - lastWildCard);  
        //         } else {
        //             // wildcard is either too many characters long or just a stray percent sign
        //             wildCardCounter = 0; 
        //         }

        //         lastWildcardStart = i;
        //     }
        // }


        for (auto& s : wildcards) {
            auto pos = contents.find(s.first); 
            std::cout << pos << std::endl; 
            if (pos != std::string::npos) {
                contents.erase(pos, s.first.length()); 
                contents.insert(pos, s.second); 
            }
        }

        std::cout << contents << std::endl; 

        namedResource[file] = contents;  
    }   

    //------------------------------------------------------------------------------
    // Name: SetWildcard
    // Desc:
    //------------------------------------------------------------------------------
    void SetWildcard(const std::string& wildcard, const std::string& replacement) {
        std::cout << wildcard << std::endl; 
        wildcards[wildcard] = replacement;
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
