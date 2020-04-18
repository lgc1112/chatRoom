#ifndef MY_SERVER_H
#define MY_SERVER_H 
 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <signal.h>

#define USER_LIMIT 10
//#define BUFFER_SIZE 64
#define FD_LIMIT 65535
#define LISTEN_PORT 12345

#define LISTEN_IP "0.0.0.0"
#define MSG_LEN 64
#define RECORD_SIZE 100
struct ClientData
{
    int fd; 
    char* writeBuf; 
};
struct HistoryRecord
{
    int size; //所有记录的数量
    int sendIdx; //当前发送数据所在的位置index(第sendIdx条消息)
    char records[RECORD_SIZE * MSG_LEN]; //记录历史数据
    HistoryRecord() : size(0), sendIdx(-1){
        memset(records, '\0', sizeof(records));
    }

    int getSendIdx(){
        return sendIdx;
    }

    int increaseSendIdx(){ //sendIdx+MSG_LEN为下一条消息所在的位置
        if(size < RECORD_SIZE){
            size++;
            sendIdx++;
        }else{
            sendIdx++;
            if(sendIdx == RECORD_SIZE) //在第100条消息，循环使用
                sendIdx = 0;          
        }
        return sendIdx;
    }
};
class MyServer
{
public:
    static bool stop;
    MyServer(const char* ip, int port) : listenPort(port), userCounter(0){
        strcpy(listenIp, ip);
        clientfds = *(new std::vector<ClientData*>(USER_LIMIT));
    }
    ~MyServer(){ 
        //delete &clientfds;
    }
    void startListening();

private:
    const char sendRequestBuff[MSG_LEN] = "send OK";
    int listenPort;
    char listenIp[32];
    int userCounter;
    bool isSending = false;
    int sendNum = 0;
    HistoryRecord mHistoryRecord;
    

    std::vector<ClientData*> clientfds; 
    std::unordered_map<int, int> fd2IdxMap;

    int setnonblocking(int fd);
    int addClientFd(int& epollFd, int& fd);
    int deleteClientFd(int epollFd, int fd);
    int deleteAllClientFd(int epollFd);
    void sendHistoryRecord(int fd);
};
#endif