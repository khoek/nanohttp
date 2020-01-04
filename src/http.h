#ifndef HTTP_H
#define HTTP_H

#include <cstdio>
#include <cstdarg>

#define METHOD_GET 0
#define METHOD_POST 1

extern const char * HTTP_METHODS[];

class HttpRequest {
    public:
        HttpRequest(int method, char *uri, char *version);
        ~HttpRequest();
        int method;
        char *uri;
        char *version;
};

class HttpResponse {
    public:
        HttpResponse(char *directive, char *body);
        ~HttpResponse();
        char *version;
        char *directive;
        char *body;
};

typedef HttpResponse * (*HttpRequestHandler)(HttpRequest *);

class HttpServer;

class ClientImpl {
    public:
        void close();
        bool isOpen();
        virtual char * readLine(HttpServer *server) = 0;
        virtual void transmit(HttpServer *server, char *response) = 0;

    private:
        volatile bool open = true;
	    virtual void doClose() = 0;
};

class ServerImpl {
    public:
        ServerImpl(bool loggingEnabled);
        void logf(const char *format, ...);
        void vlogf(const char *format, va_list args);
        void setLoggingEnabled(bool loggingEnabled);
        virtual ClientImpl * accept() = 0;
        virtual void doLog(const char *line) = 0;

    private:
        volatile bool loggingEnabled = false;
};

class HttpServer {
    public:
        HttpServer(ServerImpl *impl, HttpRequestHandler handler);
        void run();
        void halt();
        void logf(const char *format, ...);
        void setLoggingEnabled(bool loggingEnabled);
        
    private:
        ServerImpl *impl;
        HttpRequestHandler handler;
        volatile bool running = true;
        HttpRequest * parseRequest(ClientImpl *client);
        char * buildResponse(HttpResponse *response);
};

HttpResponse * makeErrorResponse(const char *error);

#include "impl/unix.h"

#endif
