#ifndef IMPL_UNIX_H
#define IMPL_UNIX_H

#include "../http.h"

class UNIXClientImpl : public ClientImpl {
    public:
        UNIXClientImpl(int fd);
        ~UNIXClientImpl();
        char * readLine(HttpServer *server) override;
        void transmit(HttpServer *server, char *response) override;

    private:
        int fd;
	void doClose() override;
};

class UNIXServerImpl : public ServerImpl {
    public:
        UNIXServerImpl(bool loggingEnabled, int port);
        ClientImpl * accept() override;
        void doLog(const char *line) override;

    private:
        int sockfd;
};

#endif
