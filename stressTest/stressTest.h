#ifndef STRESS_TEST_H
#define STRESS_TEST_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h> 
#include <vector>
#include <unordered_map>
#include <iostream>
#include <signal.h>

class stressTest{
public:
    static bool stop;
    stressTest(int testNum, const char* serverIp, int serverPort) : testNum(testNum), serverPort(serverPort){
        strcpy(this->serverIp, serverIp);
        testFds = new std::vector<int>(testNum);
        for(int i = 0; i < testNum; i++){ //默认设置为-1
            (*testFds)[i] = -1;
            
        }
    }
    ~stressTest(){        
        delete testFds;
    }
    void startTest();
private:
    int testNum;
    int serverPort;
    char serverIp[32]; 
    std::vector<int>* testFds;
    std::unordered_map<int, int> fd2IdxMap;

    int setnonblocking(int fd);
    void addfd(int epoll_fd, int fd);
    bool write_nbytes(int sockfd, const char* buffer, int len);
    bool read_once(int sockfd, char* buffer, int len);
    int start_conn(int epoll_fd);
    void close_conn(int epoll_fd, int sockfd);
    void closeAllConn(int epoll_fd); 
    bool write_once(int sockfd, const char* buffer, int len); 
};
#endif
