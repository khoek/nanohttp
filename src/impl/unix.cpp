#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <netinet/in.h>

#include "../http.h"

#define BUFF_MAX 500

UNIXClientImpl::UNIXClientImpl(int fd) {
    this->fd = fd;
}

UNIXClientImpl::~UNIXClientImpl() {
    this->doClose();
}

void UNIXClientImpl::doClose() {
    ::close(this->fd);
}

char * UNIXClientImpl::readLine(HttpServer *server) {
    char *buff = (char *) malloc(BUFF_MAX + 1);
    
    bool carReturn = false;
    int rd = 0;
    while(rd < BUFF_MAX) {
        int ret = recv(this->fd, buff + rd, 1, 0);
        if(ret == -1) {
            server->logf("UNIXClientImpl: recv() error");
            return NULL;
        }
        if(ret == 0) {
            server->logf("UNIXClientImpl: socket closed unexpectedly");
            return NULL;
        }
        
        if(carReturn && buff[rd] == '\n') {
            break;
        }
        
        carReturn = (buff[rd] == '\r');        
        rd += ret;
    }

    buff[rd - 1] = '\0';
    return buff;
}

void UNIXClientImpl::transmit(HttpServer *server, char *response) {
    send(this->fd, response, strlen(response), 0);
}

UNIXServerImpl::UNIXServerImpl(bool loggingEnabled, int port) : ServerImpl(loggingEnabled) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        this->logf("socket() failed: %d", errno); 
        exit(1); 
    }
      
      struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(port);
  
    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) != 0) {
        this->logf("bind() failed: %d", errno);
        exit(1);
    }
  
    if (listen(sockfd, 5) != 0) {
        this->logf("listen() failed: %d", errno);
        exit(1);
    }
    
    this->logf("UNIXServerImpl: running");
}

ClientImpl * UNIXServerImpl::accept() {
    struct sockaddr_in cli; 
    socklen_t len = sizeof(cli);
    int connfd = -1;
    while(connfd < 0) {
        connfd = ::accept(sockfd, (struct sockaddr *) &cli, &len);
        if (connfd < 0) {
            this->logf("accept() failed: %d", errno);
            exit(1);
        }
    }
    
    return new UNIXClientImpl(connfd);
}

void UNIXServerImpl::doLog(const char *line) {
    printf("%s\n", line);
}
