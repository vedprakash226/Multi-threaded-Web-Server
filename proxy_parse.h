#ifndef PROXY_PARSE
#define PROXY_PARSE

#include <string>
#include <vector>
#include <iostream>

class ParsedRequest {
public:
    std::string method;
    std::string protocol;
    std::string host;
    std::string port;
    std::string path;
    std::string version;
    
    struct ParsedHeader {
        std::string key;
        std::string value;
    };
    
    std::vector<ParsedHeader> headers;

    ParsedRequest();
    ~ParsedRequest();

    int parse(const char *buf, int buflen);
    
    // Unparse headers into a buffer. Returns 0 on success, -1 on failure.
    int unparse_headers(char *buf, size_t buflen);

    // Set a header. Overwrites if exists.
    void setHeader(std::string key, std::string value);
    
    // Get a header value. Returns empty string if not found.
    std::string getHeader(std::string key);
    
    // Remove a header.
    void removeHeader(std::string key);

private:
    size_t headersLen();
};

#endif
