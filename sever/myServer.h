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
#include <fstream>
#include <vector>
#include <unordered_map>
#include <signal.h>

#define USER_LIMIT 10 //连接的用户数量限制
#define MSG_LEN 64 //每条消息的长度，为解决TCP粘包和拆包问题，将每条消息的长度固定为MSG_LEN
#define RECORD_SIZE 100 //记录历史数据的数量
#define FNAME  "./historyRecord" //记录历史数据的文件

#define MSG_LEN 64 //每条消息的长度，为解决TCP粘包和拆包问题，将每条消息的长度固定为MSG_LEN

struct ClientData
{
    int fd; //用户fd
    char* writeBuf; //待写指针
};

struct HistoryRecord //记录历史信息的数据结构
{
private:
    int size; //所有记录的数量
    int sendIdx; //当前发送数据所在的位置index(第sendIdx条消息)

public:
    char records[RECORD_SIZE * MSG_LEN]; //记录历史数据

    HistoryRecord() : size(0), sendIdx(-1){
        memset(records, '\0', sizeof(records));
    }

    int getSize(){
        return size;
    }

     int getSendIdx(){
        return sendIdx;
    }

    int increaseSendIdx(){ //sendIdx+1为下一条消息所在的位置
        if(size < RECORD_SIZE){
            size++;
            sendIdx++;
        }else{
            sendIdx++;
            if(sendIdx == RECORD_SIZE) //在第100条消息，循环使用
                sendIdx = 0;          
        }
        return sendIdx;//返回新的发送消息的index
    }
};

class MyServer
{
public:
    MyServer(const char* ip, int port, int pipeFd) : listenPort(port), userCounter(0), pipeFd(pipeFd){
        strcpy(listenIp, ip);
        clientfds = *(new std::vector<ClientData*>(USER_LIMIT));
        mHistoryRecord = new HistoryRecord();
        loadRecords(mHistoryRecord);//加载历史记录文件到mHistoryRecord

    }
    ~MyServer(){ 
        storeRecords(mHistoryRecord);//保存mHistoryRecord到历史记录文件
        delete mHistoryRecord;
        //delete &clientfds;
    }
    void startListening();

private:
    bool stop = false; //停止标志
    int pipeFd; //信号通知管道
    int listenPort; //监听端口
    char listenIp[32]; //监听ip
    int userCounter; //当前连接的用户数量
    bool isSending = false; //是否处于数据正在分发的状态
    int sendNum = 0;//已发送数据的数量
    HistoryRecord* mHistoryRecord; //历史记录表

    std::vector<ClientData*> clientfds; //记录当前所有连接的fd
    std::unordered_map<int, int> fd2IdxMap;//保存fd到clientfds的index的映射关系

    int setnonblocking(int fd);//设置非阻塞
    int addClientFd(int epollFd, int fd);//添加用户fd到epoll和clientfds，建立fd到clientfds的index的映射关系
    int addPipeFd(int epollFd, int fd);//添加监听fd到epoll监听
    int deleteClientFd(int epollFd, int fd);//关闭指定用户fd
    int deleteAllClientFd(int epollFd);//关闭所有用户fd
    void sendHistoryRecord(int fd); //发送最近100条历史记录到对应fd
    void loadRecords(HistoryRecord* mHistoryRecord);//加载历史记录到mHistoryRecord
    void storeRecords(HistoryRecord* mHistoryRecord);//保存历史记录到mHistoryRecord
};


#endif