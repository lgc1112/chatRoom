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

#define MSG_LEN 64 //每条消息的长度，为解决TCP粘包和拆包问题，将每条消息的长度固定为MSG_LEN
#define TEST_TIME 10 //测试的时间

class StressTest{
public:
    StressTest(int testNum, const char* serverIp, int serverPort, int pipeFd) : testNum(testNum), serverPort(serverPort), pipeFd(pipeFd){
        strcpy(this->serverIp, serverIp);
        testFds = new std::vector<int>(testNum);
        for(int i = 0; i < testNum; i++){ //默认设置为-1
            (*testFds)[i] = -1;            
        }
    }
    ~StressTest(){        
        delete testFds; //自动回收
    }
    void startTest();
private:
    bool stop = false; //测试结束标志
    bool success = false; //测试成功标志
    int testNum; //测试客户的数量
    int serverPort; //服务器端口号
    char serverIp[32]; //服务器ip
    int pipeFd; //接收信号的管道
    std::vector<int>* testFds; //记录当前所有测试连接的fd
    std::unordered_map<int, int> fd2IdxMap; //保存fd到clientfds的index的映射关系

    int setnonblocking(int fd); //设置非阻塞
    void addClientFd(int epollFd, int fd); //添加测试fd到epoll
    void addPipeFd(int epollFd, int fd); //添加监听fd到epoll
    bool readOnce(int sockfd, char* buffer, int len);//读取一次数据
    int startConn(int epollFd);//建立测试连接
    void closeConn(int epollFd, int sockfd);//关闭指定测试连接
    void closeAllConn(int epollFd); //关闭所有测试连接
    bool writeOnce(int sockfd, const char* buffer, int len); //写一次数据
};
#endif
