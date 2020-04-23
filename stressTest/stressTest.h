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

#define MSG_MAX_LEN 64 //每条消息的长度，为解决TCP粘包和拆包问题，将每条消息的长度固定为MSG_MAX_LEN
#define TEST_TIME 10 //测试的时间
#define REC_LEN 2048

//压力测试时请注释掉
//#define __NEEDPRINT //用于控制是否打印发送或接收到的数据，关闭打印使得测试请求数量更快，因为打印的系统调用比较耗时
#ifdef __NEEDPRINT
#define PRINT_DATA(format,...) printf (format, ##__VA_ARGS__)
//可打印文件名、行号
//#define DEBUG(format,...) printf("FILE: "__FILE__", LINE: %d: "format"/n", __LINE__, ##__VA_ARGS__)
#else
#define PRINT_DATA(format, ...)
#endif 


struct clientData{
    int fd;
    bool isRequesting;//该客户端是否处于发送了请求，但是还没有收到ask阶段
    char receiveBuf[REC_LEN];
    int receivedLen; //receiveBuf中保存的已接收的数据的长度
    clientData(int fd) : fd(fd), isRequesting(false), receivedLen(0){ }
    
};

class StressTest{
public:
    StressTest(int testNum, const char* serverIp, int serverPort, int pipeFd) : testNum(testNum), serverPort(serverPort), pipeFd(pipeFd){
        strcpy(this->serverIp, serverIp);
        testClientDatas = *(new std::vector<clientData*>(testNum));
        for(int i = 0; i < testNum; i++){ //默认设置为-1
            clientData* tmp = new clientData(-1);
            testClientDatas[i] = tmp;            
        }
    }
    ~StressTest(){     
        for(int i = 0; i < testNum; i++){ //默认设置为-1
            delete testClientDatas[i]; 
        }   
        //delete testClients; //自动回收
    }
    void startTest();
    void init();
private:
    int epollFd;
    bool stop = false; //测试结束标志
    bool success = false; //测试成功标志
    int testNum; //测试客户的数量
    int serverPort; //服务器端口号
    char serverIp[32]; //服务器ip
    int pipeFd; //接收信号的管道
    long long successRequestNum = 0;//成功请求的数量
    std::vector<clientData*> testClientDatas; //记录当前所有测试连接的fd
    std::unordered_map<int, int> fd2IdxMap; //保存fd到clientfds的index的映射关系 

    int setnonblocking(int fd); //设置非阻塞
    void addClientFd(int fd); //添加测试fd到epoll
    void addPipeFd(int fd); //添加监听fd到epoll
    bool readOnce(int sockfd, char* buffer, int len);//读取一次数据
    int startConn();//建立测试连接
    void closeConn(int sockfd);//关闭指定测试连接
    void closeAllConn(); //关闭所有测试连接
    bool writeOnce(int sockfd, const char* buffer, int len); //写一次数据
    int handleMsg(char* buf, int len, int sockfd);//处理接收到的数据
    int pack(char* dst, char* src);//封包函数
};
#endif
