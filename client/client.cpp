#define _GNU_SOURCE 1
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

#define MSG_LEN 64
#define SERVER_PORT 12345
#define SERVER_IP "0.0.0.0"

int main( void )
{  
    const char* ip = SERVER_IP;
    int port = SERVER_PORT;

    struct sockaddr_in server_address;
    bzero( &server_address, sizeof( server_address ) );
    server_address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &server_address.sin_addr );
    server_address.sin_port = htons( port );

    int connFd = socket( PF_INET, SOCK_STREAM, 0 );
    int lowat = MSG_LEN; //设置发送和接收的低水位值，都是MSG_LEN，只有发送或接受缓冲区大于MSG_LEN才触发读写事件
    setsockopt(connFd, SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat));
    setsockopt(connFd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
    
    assert( connFd >= 0 );
    if ( connect( connFd, ( struct sockaddr* )&server_address, sizeof( server_address ) ) < 0 )
    {
        printf( "connection failed\n" );
        close( connFd );
        return 1;
    }

    int epollFd = epoll_create(100); 
    epoll_event events[ 10000 ];

 
    char readBuff[MSG_LEN];
    char sendBuff[MSG_LEN];
    int pipefd[2];
    int ret = pipe( pipefd );
    bool isSending = false;
    assert( ret != -1 );

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = STDIN_FILENO;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, STDIN_FILENO, &event); 

    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = connFd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, connFd, &event); 

    bool stop = false;
    while( !stop )
    {
        int fds = epoll_wait(epollFd, events, 1000, 2000); //最多监听1000件事，最多等呆2s
        for (int i = 0; i < fds; i++)
        { 
            int tmpFd = events[i].data.fd;
            if(tmpFd == connFd && events[i].events & EPOLLRDHUP){
                printf( "server close the connection\n" );
                stop = true;
                break;
            }
            else if(tmpFd == connFd && events[i].events & EPOLLIN){
                memset( readBuff, '\0', MSG_LEN );
                recv( tmpFd, readBuff, MSG_LEN, 0 ); 
                printf( "%s\n", readBuff );
            }
            else if(tmpFd == connFd && events[i].events & EPOLLOUT){
                isSending = false;
                ret = send(tmpFd, sendBuff, MSG_LEN, 0);
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLRDHUP;
                event.data.fd = connFd;
                epoll_ctl(epollFd, EPOLL_CTL_MOD, connFd, &event);
            }
            
            
            else if(tmpFd == STDIN_FILENO && events[i].events & EPOLLIN){
                if(isSending) //等待发送完毕
                    continue;
                memset(sendBuff, '\0', MSG_LEN);
                int len = read(tmpFd, sendBuff, MSG_LEN - 1); //每条消息长度最多为MSG_LEN - 1
                if(strlen(sendBuff) == 0) //空消息不处理
                    continue;
                // printf( "read %s", sendBuff);
                if(len == 0){
                    printf( "read error\n" );
                    continue;
                }else{
                    struct epoll_event event;
                    event.events = EPOLLOUT | EPOLLRDHUP;
                    event.data.fd = connFd;
                    epoll_ctl(epollFd, EPOLL_CTL_MOD, connFd, &event);
                    isSending = true;
                }
            }
            

        }
         
    }
    
    close( connFd );
    return 0;
}
