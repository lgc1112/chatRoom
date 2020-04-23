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
#include <errno.h>

#define MSG_MAX_LEN 64 //每条消息的长度。为解决TCP粘包和拆包问题，将每条消息的长度固定为MSG_MAX_LEN
#define SERVER_PORT 12345 //服务器的端口号
#define SERVER_IP "0.0.0.0" //服务器的ip,本机

int pack(char* dst, char* src)
{
    int len = strlen(src) + 1 + 4;//加上包头后的长度 
    dst[0] = 0x11;
    dst[1] = 0x22;
    dst[2] = 0x33; 
    dst[3] = strlen(src) + 1;
    if(dst[3] + 4 > MSG_MAX_LEN) //太长了dst会溢出
        return -1;
    memcpy(dst + 4, src, dst[3] );
    return len;

}

int readBytes(int fd, char* strData, int len)
{
    if(strData == NULL)
        return -1;  
    int ret = 0;
    int recvlen = 0;
    while(recvlen < len)
    {
        ret = recv(fd, strData + recvlen, len - recvlen, 0 );
        if ( ret == -1 || ret == 0 )
        {
            if(errno != EAGAIN){ //除了无数据可读的错误外，其它错误直接return
                return ret;
                printf("error in readByte\n"); 
            }
        } 
        recvlen += ret;
    }
    return recvlen;
}

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
    // int lowat = MSG_MAX_LEN; //设置发送和接收的低水位值，都是MSG_MAX_LEN，\
    // 只有发送或接受缓冲区大于MSG_MAX_LEN才触发读写事件，因为每条消息长度固定为MSG_MAX_LEN
    // setsockopt(connFd, SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat));
    // setsockopt(connFd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
    
    assert(connFd >= 0);
    if (connect(connFd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)//建立连接
    {
        printf("connection failed\n");
        close(connFd);
        return 1;
    }

    int epollFd = epoll_create(100); 
    epoll_event events[ 10000 ];

 
    char readBuff[MSG_MAX_LEN];
    char sendBuff[MSG_MAX_LEN] = {0x11, 0x22, 0x33}; //填好包头数据
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
            else if(tmpFd == connFd && events[i].events & EPOLLIN){//接收到服务器消息 
                memset(readBuff, '\0', MSG_MAX_LEN);
                int ret = readBytes(connFd, readBuff, 4);//循环读取长度为4的包头，直到接收了一定长度的数据或者网络出错才返回 
                if(ret != 4){ //包头不对，出现了错误 
                    printf("error & delete a client\n"); 
                    continue;
                }
                if(readBuff[0] == 0x11 && readBuff[1] == 0x22 && readBuff[2] == 0x33){//包头关键字是否对
                    int msgLen = readBuff[3]; //数据长度               
                    ret = readBytes(connFd, readBuff + 4, msgLen);//循环读取长度为len的数据，直到接收了一定长度的数据或者网络出错才返回 
                    if(ret != msgLen){ 
                        printf("error & delete a client\n"); 
                        continue;
                    }
                    printf("%s\n", readBuff + 4);//读取正确，打印出来                    
                }else if(readBuff[0] == 0x11 && readBuff[1] == 0x22 && readBuff[2] == 0x22  && readBuff[3] == 0){//包头关键字是否对,这是ack信息
                    printf("received ask\n");  
                }else{//包头不对，出现了错误 
                    printf("error & delete a client\n"); 
                    continue;
                } 
            }
            else if(tmpFd == connFd && events[i].events & EPOLLOUT){//可写事件
                send(tmpFd, sendBuff, strlen(sendBuff) + 1, 0);//发送数据到对应fd，消息长度固定为MSG_MAX_LEN，
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLRDHUP;//更改为监听可读事件
                event.data.fd = connFd;
                epoll_ctl(epollFd, EPOLL_CTL_MOD, connFd, &event);
                isSending = false;//数据发送状态结束
            }
            
            
            else if(tmpFd == STDIN_FILENO && events[i].events & EPOLLIN){//键盘输入事件
                if(isSending) //等待数据发送完毕，防止sendBuff没有发送就被覆盖了
                    continue;
                memset(sendBuff + 4, '\0', MSG_MAX_LEN);//清楚除包头外的数据
                int len = read(tmpFd, sendBuff + 4, MSG_MAX_LEN - 5); //每条输入消息长度最多为MSG_MAX_LEN - 5，\
                过长的消息自动分段发送，过短消息则后面补\0，保存到sendBuff
                if(strlen(sendBuff + 4) == 0) //空消息不处理
                    continue;
                sendBuff[3] = strlen(sendBuff + 4) + 1; //包体的长度
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
