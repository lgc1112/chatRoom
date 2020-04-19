#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define MSG_LEN 64 //每条消息的长度。为解决TCP粘包和拆包问题，将每条消息的长度固定为MSG_LEN
#define SERVER_PORT 12345 //服务器的端口号
#define SERVER_IP "0.0.0.0" //服务器的ip,本机

int main(void)
{  
    const char* ip = SERVER_IP;
    int port = SERVER_PORT;

    struct sockaddr_in server_address; //ip地址和端口转换
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int connFd = socket(PF_INET, SOCK_STREAM, 0);//新建ipv4 TCP流socket
    int lowat = MSG_LEN; //设置发送和接收的低水位值，都是MSG_LEN，\
    只有发送或接受缓冲区大于MSG_LEN才触发读写事件，因为每条消息长度固定为MSG_LEN
    setsockopt(connFd, SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat));
    setsockopt(connFd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
    
    assert(connFd >= 0);
    if (connect(connFd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)//建立连接
    {
        printf("connection failed\n");
        close(connFd);
        return 1;
    }

    int epollFd = epoll_create(100); 
    epoll_event events[ 10000 ];

 
    char readBuff[MSG_LEN];
    char sendBuff[MSG_LEN]; 
    bool isSending = false; 

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = STDIN_FILENO;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, STDIN_FILENO, &event); //监听输入事件STDIN_FILENO

    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = connFd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, connFd, &event);  //监听连接socket

    bool stop = false;
    while(!stop)
    {
        int fds = epoll_wait(epollFd, events, 1000, 2000); //最多监听1000件事，最多等呆2s
        for (int i = 0; i < fds; i++)
        { 
            int tmpFd = events[i].data.fd;
            if(tmpFd == connFd && events[i].events & EPOLLRDHUP){//服务器断开连接
                printf("server close the connection\n");
                stop = true;
                break;
            }
            else if(tmpFd == connFd && events[i].events & EPOLLIN){//接收到连接消息
                memset(readBuff, '\0', MSG_LEN);
                recv(tmpFd, readBuff, MSG_LEN, 0); //每条消息长度固定为MSG_LEN，读取到readBuff
                printf("%s\n", readBuff);
            }
            else if(tmpFd == connFd && events[i].events & EPOLLOUT){//可写事件
                send(tmpFd, sendBuff, MSG_LEN, 0);//发送数据到对应fd，消息长度固定为MSG_LEN，
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLRDHUP;//更改为监听可读事件
                event.data.fd = connFd;
                epoll_ctl(epollFd, EPOLL_CTL_MOD, connFd, &event);
                isSending = false;//数据发送状态结束
            }
            
            
            else if(tmpFd == STDIN_FILENO && events[i].events & EPOLLIN){//键盘输入事件
                if(isSending) //等待数据发送完毕，防止sendBuff没有发送就被覆盖了
                    continue;
                memset(sendBuff, '\0', MSG_LEN);
                int len = read(tmpFd, sendBuff, MSG_LEN - 1); //每条输入消息长度最多为MSG_LEN - 1，\
                过长的消息自动分段发送，过短消息则后面补\0，保存到sendBuff
                if(strlen(sendBuff) == 0) //空消息不处理
                    continue;
                // printf("read %s", sendBuff);
                if(len == 0){
                    printf("read error\n");
                    continue;
                }else{
                    struct epoll_event event;
                    event.events = EPOLLOUT | EPOLLRDHUP; //更改为监听可写事件
                    event.data.fd = connFd;
                    epoll_ctl(epollFd, EPOLL_CTL_MOD, connFd, &event);
                    isSending = true;//处于数据发送状态
                }
            }
            

        }
         
    }
    
    close(connFd);
    return 0;
}
