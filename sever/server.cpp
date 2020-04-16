#define _GNU_SOURCE 1
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
#define USER_LIMIT 10
//#define BUFFER_SIZE 64
#define FD_LIMIT 65535
#define LISTEN_PORT 12345
#define LISTEN_IP "0.0.0.0"
#define MSG_LEN 64

struct client_data
{
    int fd;
    sockaddr_in address;
    char* write_buf;
    char buf[ MSG_LEN ];
};

class server
{
public:
    server(const char* ip, int port) : listenPort(port), userCounter(0){
        strcpy(listenIp, ip);
        clientfds = *(new std::vector<client_data*>(USER_LIMIT));
    }
    ~server(){
        delete &clientfds;
    }
    void startListening();

private:
    const char sendRequestBuff[MSG_LEN] = "send OK";
    int listenPort;
    char listenIp[32];
    int userCounter;
    bool isSending = false;
    int sendNum = 0;

    std::vector<client_data*> clientfds; 
    std::unordered_map<int, int> fd2IdxMap;

    int setnonblocking(int fd);
    int addClientFd(int& epollFd, int& fd, struct sockaddr_in& client_address);
    int deleteClientFd(int epollFd, int fd);
};


int server::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}


int server::addClientFd(int& epollFd, int& fd, struct sockaddr_in& client_address)
{
    client_data* tmp = new client_data;
    tmp->address = client_address; 
    tmp->fd = fd;
    clientfds[userCounter] = tmp;
    fd2IdxMap[fd] = userCounter; //建立fd到clientfds数组idx的映射关系
    userCounter++;
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    event.data.fd = fd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);//注册监听输入等事件
    setnonblocking(fd);
}

int server::deleteClientFd(int epollFd, int sockfd)
{
    userCounter--; 
    if(sendNum >= userCounter)//如果一个用户退出了，并且发送的次数大于等于剩下的用户数，则需要把正在发送状态置false
        isSending = false;
    int idx = fd2IdxMap[sockfd]; //要删除的fd在clientfds数组对应的对应的idex
    delete clientfds[idx]; //删除该fd对应的client_data
    clientfds[idx] = clientfds[userCounter];  //将clientfds数组中的最后一个有效clientdata移动到删除的idx
    fd2IdxMap[clientfds[userCounter]->fd] = idx; //更新最后一个有效clientdata对应fd的idx
    fd2IdxMap.erase(sockfd); //该fd已删除，删除映射关系
    epoll_ctl(epollFd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);

}



void server::startListening()
{
    char sendRequestBuff[MSG_LEN] = "send OK";
    int ret = 0;

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, listenIp, &address.sin_addr);
    address.sin_port = htons(listenPort);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int lowat = MSG_LEN; //设置发送和接收的低水位值，都是MSG_LEN，只有发送或接受缓冲区大小为MSG_LEN才触发读写事件，accept返回的sock将会自动继承这个值
    setsockopt(listenfd, SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat));
    setsockopt(listenfd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
    
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);
    std::cout << "listening... " << std::endl;
    
    int epollFd = epoll_create(100);
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLERR;
    event.data.fd = listenfd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, listenfd, &event); 
    
    epoll_event events[ 1000 ];
    while(true){
        int fds = epoll_wait(epollFd, events, 1000, 2000); //最多监听1000件事，最多等呆2s
        for (int i = 0; i < fds; i++)
        {   
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd && events[i].events & EPOLLIN) //新连接
            { 
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                
                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if(userCounter >= USER_LIMIT)
                {
                    const char* info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                
                addClientFd(epollFd, connfd, client_address);
                
                // users[connfd].address = client_address;
                // fds[user_counter].fd = connfd;
                // fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                // fds[user_counter].revents = 0;
                printf("comes a new user, now have %d users\n", userCounter);
                
            }
            else if(events[i].events & EPOLLERR)
            {
                printf("get an error from %d\n", sockfd);
                deleteClientFd(epollFd, sockfd);
                char errors[ 100 ];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(errors);
                if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0)
                {
                    printf("get socket option failed\n");
                }
                continue;
            }
            else if(events[i].events & EPOLLRDHUP)//TCP连接被对方关闭或者对方关闭了写操作
            {
                deleteClientFd(epollFd, sockfd);
                
                // users[fds[i].fd] = users[fds[user_counter].fd];
                // close(fds[i].fd);
                // fds[i] = fds[user_counter];
                // i--;
                // user_counter--;
                printf("a client left\n");
            }
            else if(events[i].events & EPOLLIN)
            {                
                //int connfd = fds[i].fd;
                int idx = fd2IdxMap[sockfd];//该fd在clientfds数组对应的对应的idex
                client_data* data = clientfds[idx];
                memset(data->buf, '\0', MSG_LEN);
                ret = recv(sockfd, data->buf, MSG_LEN, 0);
                //memset(users[connfd].buf, '\0', BUFFER_SIZE);
                //ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1, 0);
                printf("get %d bytes of client data %s from %d\n", ret, data->buf, sockfd);
                if(ret < 0)
                {
                    if(errno != EAGAIN) //除了try again 的错误外，其它错误直接关闭fd
                    {
                        deleteClientFd(epollFd, sockfd);
                        printf("error & delete a client\n");
                        // close(connfd);
                        // users[fds[i].fd] = users[fds[user_counter].fd];
                        // fds[i] = fds[user_counter];
                        // i--;
                        // user_counter--;
                    }
                }
                else if(ret == 0)
                {
                    deleteClientFd(epollFd, sockfd);
                    printf("code should not come to here\n");
                }
                else
                {
                    if(isSending) //数据正在分发给各个客户端，需要等待数据发送结束再处理新接收新数据
                        continue;
                    for(int j = 0; j < userCounter; j++)
                    {
                        client_data* tmp = clientfds[j];
                        int fd = tmp->fd;

                        if(fd == sockfd)
                        {
                            tmp->write_buf = sendRequestBuff;

                            // fds[j].events |= ~POLLIN;
                            // fds[j].events |= POLLOUT;
                            // users[fds[j].fd].write_buf = sendRequestBuff;                             
                        }else{
                            tmp->write_buf = data->buf;
                        }
                        
                        // fds[j].events |= ~POLLIN;
                        // fds[j].events |= POLLOUT;
                        // users[fds[j].fd].write_buf = users[connfd].buf;
                        
                        struct epoll_event event;
                        event.events = EPOLLOUT | EPOLLRDHUP | EPOLLERR;
                        event.data.fd = fd;
                        epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event);
                    }
                    isSending = true;//开始分发数据给各个客户端，当前已发送次数置0
                    sendNum = 0;
                }
            }
            else if(events[i].events & EPOLLOUT)
            { 
                sendNum++;//发送次数加1
                if(sendNum >= userCounter) //已发送userCounter次，则将isSending状态置false；
                    isSending = false;
                int idx = fd2IdxMap[sockfd];//该fd在clientfds数组对应的对应的idex
                client_data* data = clientfds[idx];
                if(! data->write_buf)
                {
                    printf("write_buf is NULL\n");
                    continue;
                }
                ret = send(sockfd, data->write_buf, MSG_LEN, 0);
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl(epollFd, EPOLL_CTL_MOD, sockfd, &event);
                // int connfd = fds[i].fd;
                // if(! users[connfd].write_buf)
                // {
                //     continue;
                // }
                // ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                // users[connfd].write_buf = NULL;
                // fds[i].events |= ~POLLOUT;
                // fds[i].events |= POLLIN;
            }






        }


    }
    //delete [] users;
    close(listenfd);
}
int main(void)
{
    const char* ip = LISTEN_IP;
    int port = LISTEN_PORT;
    server mServer(ip, port);
    mServer.startListening();
    return 0;
}
