#include <cstdlib>
#include <cstring>
#include <cstdarg>

#include "http.h"

#define BUFF_MAX 1000
#define LOGBUFF_SIZE 500
#define LINE_SEP "\r\n"

const char * HTTP_METHODS[] = { "GET", "POST" };

HttpResponse::HttpResponse(char *directive, char *body) {
    this->version = strdup("HTTP/1.0");
    this->directive = directive;
    this->body = body;
}

HttpResponse::~HttpResponse() {
    free(this->version);
    free(this->directive);
    free(this->body);
}

HttpRequest::HttpRequest(int method, char *uri, char *version) {
    this->method = method;
    this->uri = uri;
    this->version = version;
}

HttpRequest::~HttpRequest() {
    free(this->uri);
    free(this->version);
}

HttpRequest * HttpServer::parseRequest(ClientImpl *client) {
    char *line = client->readLine(this);
    if(!line) {
        client->close();
        return NULL;
    }

    char *copy = (char *) malloc(strlen(line) + 1);
    strcpy(copy, line);
    
    char *method = strdup(strtok(line, " "));
    char *uri = strdup(strtok(NULL, " "));
    char *version = strdup(strtok(NULL, " "));
    
    if(method == NULL
        || uri == NULL
        || version == NULL
        || strtok(NULL, " ") != NULL) {
        this->logf("parseRequest(): bad query: %s", copy);
        free(line);
        free(copy);
        free(method);
        free(uri);
        free(version);
        return NULL;
    }
    
    free(line);
    free(copy);
    
    int methodNum;
    for(methodNum = 0; methodNum < sizeof(HTTP_METHODS) / sizeof(char *); methodNum++) {
        if(!strcmp(method, HTTP_METHODS[methodNum])) {
            break;
        }
    }
    
    if(methodNum == sizeof(HTTP_METHODS) / sizeof(char *)) {
        this->logf("parseRequest(): bad method: %s", method);
        free(method);
        free(uri);
        free(version);
        return NULL;
    }
    
    while(line = client->readLine(this)) {
        if(strlen(line) == 0) {
            free(line);
            break;
        }
        free(line);
    }
    
    if(!line) {
        this->logf("parseRequest(): no header-ending newline");
        return NULL;
    }

    return new HttpRequest(methodNum, uri, version);
}

static void reverse(char s[]) {
    int i = 0;
    int j = strlen(s) - 1;
    for (; i < j; i++, j--) {
        char c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}  

static void itoa(int n, char s[]) {
    int sign = n < 0;
    n = sign ? -n : n;

    int i = 0;
    do {
        s[i++] = (n % 10) + '0';
        n /= 10;
    } while (n > 0);
    if (sign < 0) {
        s[i++] = '-';
    }
    s[i] = '\0';
    reverse(s);
}

static char * buffAppend(char *buff, int *buffLen, const char *src) {
    int len = strlen(src);
    if(len > *buffLen) {
        return NULL;
    }

    *buffLen -= len;

    memcpy(buff, src, len);
    return buff + len;
}

char * HttpServer::buildResponse(HttpResponse *response) {
    int buffLen = BUFF_MAX;
    char *buff = (char *) malloc(buffLen);
    char *orig = buff;
    if(!(buff = buffAppend(buff, &buffLen, response->version))) return NULL;
    if(!(buff = buffAppend(buff, &buffLen, " "))) return NULL;
    if(!(buff = buffAppend(buff, &buffLen, response->directive))) return NULL;
    if(!(buff = buffAppend(buff, &buffLen, LINE_SEP))) return NULL;
    
    if(!(buff = buffAppend(buff, &buffLen, "Content-Length: "))) return NULL;
    char strLen[100];
    itoa(strlen(response->body), strLen);
    if(!(buff = buffAppend(buff, &buffLen, strLen))) return NULL;
    if(!(buff = buffAppend(buff, &buffLen, LINE_SEP))) return NULL;
    
    if(!(buff = buffAppend(buff, &buffLen, LINE_SEP))) return NULL;
    
    if(!(buff = buffAppend(buff, &buffLen, response->body))) return NULL;
    if(!(buff = buffAppend(buff, &buffLen, LINE_SEP))) return NULL;
    if(!(buff = buffAppend(buff, &buffLen, LINE_SEP))) return NULL;
    
    return orig;
}

HttpResponse * makeErrorResponse(const char *error) {
    return new HttpResponse(strdup(error), strdup("<html><body>Error</body></html>"));
}

HttpServer::HttpServer(ServerImpl *impl, HttpRequestHandler handler) {
    this->impl = impl;
    this->handler = handler;
}

void HttpServer::run() {
    while(this->running) {
        ClientImpl *client = impl->accept();
        if(client == NULL) {
            this->logf("HttpServer: no more clients, halting");
            this->halt();
            continue;
        }
        
        HttpRequest *request = this->parseRequest(client);
        HttpResponse *response;
        if(request == NULL) {
            this->logf("HttpServer: bad request");

            if(!client->isOpen()) {
                delete client;
                continue;
            }

            response = makeErrorResponse("400 Bad Request");
        } else {        
            this->logf("HttpServer: servicing request");
            response = this->handler(request);
            delete request;
        }

        if(response == NULL) {
            this->logf("HttpServer: null response for request");
            response = makeErrorResponse("500 Internal Server Error");
        }
        
        char *responseBuff = this->buildResponse(response);
        delete response;
        if(responseBuff == NULL) {
            this->logf("HttpServer: could not build response");
            delete client;
            continue;
        }
        
        client->transmit(this, responseBuff);
        free(responseBuff);
        
        delete client;
    }
}

void HttpServer::halt() {
    this->running = false;
}

void HttpServer::setLoggingEnabled(bool loggingEnabled) {
    this->impl->setLoggingEnabled(loggingEnabled);
}

void HttpServer::logf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    this->impl->vlogf(format, args);

    va_end(args);
}

ServerImpl::ServerImpl(bool loggingEnabled) {
    this->setLoggingEnabled(loggingEnabled);
}

void ServerImpl::setLoggingEnabled(bool loggingEnabled) {
    this->loggingEnabled = loggingEnabled;
}

void ServerImpl::logf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    this->vlogf(format, args);

    va_end(args);
}

void ServerImpl::vlogf(const char *format, va_list args) {
    if(this->loggingEnabled) {
        char buff[LOGBUFF_SIZE];
        vsnprintf(buff, LOGBUFF_SIZE, format, args);
        this->doLog(buff);
    }
}

void ClientImpl::close() {
    this->open = false;
    this->doClose();
}

bool ClientImpl::isOpen() {
    return open;
}
