#include "proxy_parse.h"
#include <sstream>
#include <cstring>
#include <algorithm>

ParsedRequest::ParsedRequest() {}

ParsedRequest::~ParsedRequest() {}

void ParsedRequest::setHeader(std::string key, std::string value) {
    for (auto& header : headers) {
        if (header.key == key) {
            header.value = value;
            return;
        }
    }
    headers.push_back({key, value});
}

std::string ParsedRequest::getHeader(std::string key) {
    for (const auto& header : headers) {
        if (header.key == key) {
            return header.value;
        }
    }
    return "";
}

void ParsedRequest::removeHeader(std::string key) {
    auto it = headers.begin();
    while (it != headers.end()) {
        if (it->key == key) {
            it = headers.erase(it);
            return;
        } else {
            ++it;
        }
    }
}

size_t ParsedRequest::headersLen() {
    size_t len = 0;
    for (const auto& header : headers) {
        len += header.key.length() + 2 + header.value.length() + 2; // key: value\r\n
    }
    len += 2; // \r\n
    return len;
}

int ParsedRequest::unparse_headers(char *buf, size_t buflen) {
    if (headersLen() > buflen) return -1;
    
    char* current = buf;
    for (const auto& header : headers) {
        memcpy(current, header.key.c_str(), header.key.length());
        current += header.key.length();
        memcpy(current, ": ", 2);
        current += 2;
        memcpy(current, header.value.c_str(), header.value.length());
        current += header.value.length();
        memcpy(current, "\r\n", 2);
        current += 2;
    }
    memcpy(current, "\r\n", 2);
    return 0;
}

int ParsedRequest::parse(const char *buf, int buflen) {
    std::string buffer(buf, buflen);
    size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return -1;
    
    std::string requestLine = buffer.substr(0, buffer.find("\r\n"));
    std::stringstream ss(requestLine);
    
    if (!(ss >> method)) return -1;
    if (method != "GET") return -1;
    
    std::string fullAddr;
    if (!(ss >> fullAddr)) return -1;
    
    if (!(ss >> version)) return -1;
    if (version.find("HTTP/") != 0) return -1;
    
    // Parse fullAddr
    size_t protocolEnd = fullAddr.find("://");
    if (protocolEnd != std::string::npos) {
        protocol = fullAddr.substr(0, protocolEnd);
        std::string rest = fullAddr.substr(protocolEnd + 3);
        size_t pathStart = rest.find("/");
        if (pathStart == std::string::npos) {
            host = rest;
            path = "/";
        } else {
            host = rest.substr(0, pathStart);
            path = rest.substr(pathStart);
        }
    } else {
        protocol = "http";
        host = fullAddr;
        
        // If it starts with /, skip it (assuming the user is using the proxy by appending the target URL to the proxy URL)
        if (host.length() > 0 && host[0] == '/') {
            host = host.substr(1);
        }

        size_t pathStart = host.find("/");
        if (pathStart != std::string::npos) {
            path = host.substr(pathStart);
            host = host.substr(0, pathStart);
        } else {
            path = "/";
        }
    }
    
    // Parse port from host
    size_t portStart = host.find(":");
    if (portStart != std::string::npos) {
        port = host.substr(portStart + 1);
        host = host.substr(0, portStart);
    }
    
    // Parse headers
    size_t pos = buffer.find("\r\n") + 2;
    while (pos < headerEnd) {
        size_t lineEnd = buffer.find("\r\n", pos);
        if (lineEnd == std::string::npos) break;
        
        std::string line = buffer.substr(pos, lineEnd - pos);
        size_t colon = line.find(":");
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2); // Skip ": "
            setHeader(key, value);
        }
        pos = lineEnd + 2;
    }
    
    return 0;
}
