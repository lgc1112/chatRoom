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

#define TEST_NUM 10
#define TEST_TIME 10 //

class stressTest{
public:
    static bool stop;
    static bool success;
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
    bool readOnce(int sockfd, char* buffer, int len);
    int startConn(int epoll_fd);
    void closeConn(int epoll_fd, int sockfd);
    void closeAllConn(int epoll_fd); 
    bool writeOnce(int sockfd, const char* buffer, int len); 
};
#endif
