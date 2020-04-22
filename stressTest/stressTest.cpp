#include "stressTest.h"

void StressTest::closeAllConn(int epollFd)
{
    for(int i = 0; i < testNum; i++){ //关闭所有未关闭的连接
        if((*testFds)[i] != -1){ //连接是否存在，存在则关闭
            closeConn(epollFd, (*testFds)[i]);
            (*testFds)[i] == -1;
        }
    }
 }
int StressTest::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void StressTest::addClientFd(int epollFd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void StressTest::addPipeFd(int epollFd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

bool StressTest::writeOnce(int sockfd, const char* buffer, int len)
{
    int bytes_write = 0;
    printf("write out %d bytes to socket %d\n", len, sockfd);
    bytes_write = send(sockfd, buffer, len, 0);//写数据，这里传入的长度一般固定为MSG_LEN
    if (bytes_write == -1){   
        return false;
    }   
    else if (bytes_write == 0){   
        return false;
    }   
    return true;
}

bool StressTest::readOnce(int sockfd, char* buffer, int len)
{
    int bytes_read = 0;
    memset(buffer, '\0', len);
    bytes_read = recv(sockfd, buffer, len, 0);//读数据，这里传入的长度一般固定为MSG_LEN
    if (bytes_read == -1){ //读取数据出错
        return false;
    }
    else if (bytes_read == 0){  //服务器已关闭 
        return false;
    }
	printf("read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer);

    return true;
}

// bool StressTest::readMultiple(int sockfd, char* buffer, int len，)
// {
//     int readBytes = 0;
//     memset(buffer, '\0', len);
//     while(1){
//         readBytes = recv(sockfd, buffer, len, 0);
//         if (readBytes == -1) //读取数据出错
//         {
//             if(errno == EAGAIN) //非阻塞且无数据可读
//                 return true;
//             else
//                 return false;
//         }
//         else if (readBytes == 0)  //服务器已关闭
//         {
//             return false;
//         }else{ //成功读取数据
//             printf("read in %d bytes from socket %d with content: %s\n", readBytes, sockfd, buffer);            
//             if(sockfd == sendSocket && strcmp(buffer, sendRequestBuff) == 0){
//                 successRequestNum++;
//                 isRequesting= true;
//             }
//         } 
//     } 
// }

int StressTest::startConn(int epollFd)
{
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, serverIp, &address.sin_addr);
    address.sin_port = htons(serverPort);

    for(int i = 0; i < testNum; ++i){
        //usleep(100000);
        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        int lowat = MSG_LEN; //设置发送和接收的低水位值，都是MSG_LEN，只有发送或接受缓冲区大于MSG_LEN才触发读写事件
        setsockopt(sockfd, SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
        printf("create 1 sock\n");
        if(sockfd < 0){//建立连接失败 ret = -1; 
            ret = -1;
            continue;
        }

        if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) == 0){//建立连接 
            printf("build connection %d\n", i);
            addClientFd(epollFd, sockfd);  
            (*testFds)[i] = sockfd; //保存到测试列表 
            fd2IdxMap[sockfd] = i; //建立fd 2 idx 映射关系
        }else{
            ret = -1;//建立连接失败 ret = -1;
        }
    }
    return ret;
}

void StressTest::closeConn(int epollFd, int sockfd)
{
    stop = true; //有测试fd被关闭，测试停止
    if(fd2IdxMap.count(sockfd) == 1){ //目标fd在连接数组中，置为-1
        (*testFds)[fd2IdxMap[sockfd]] = -1;
        //fd2IdxMap.erase(sockfd);
    }
    epoll_ctl(epollFd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);
}

void StressTest::startTest()
{
    int epollFd = epoll_create(100);
    int ret = startConn(epollFd);//建立testNum个连接
    if(ret == -1){//无法建立testNum个连接，直接stop
        std::cout << "Cannot create " << testNum << " client" << std::endl; 
        stop = true;
    }
 
    addPipeFd(epollFd, pipeFd);  //监听信号通知管道

    epoll_event events[10000];
    char buffer[MSG_LEN];
    long long successRequestNum = 0;//成功请求的数量
    bool isRequesting = false;//是否处于正在请求的阶段
    int sendIdx = -1;//发送请求的fd在testFds中的index
    int sendSocket = 0;  //发送请求的socket fd
    char successRequestBuff[MSG_LEN] = "send OK"; //请求成功的返回值
    while(!stop){
        if(!isRequesting){ //请求阶段结束，准备发送新的请求
            sendIdx++;//更换下一个fd用于发送请求
            sendIdx %= testNum;
            sendSocket = (*testFds)[sendIdx];//得到下一个发送请求的发送请求的socket fd
            struct epoll_event event;
            event.events = EPOLLOUT | EPOLLET | EPOLLERR | EPOLLRDHUP;//更改为监听输出事件，准备发送请求数据
            event.data.fd = sendSocket;
            epoll_ctl(epollFd, EPOLL_CTL_MOD, sendSocket, &event);
            isRequesting = true;//请求阶段开始
        }
        int fds = epoll_wait(epollFd, events, 10000, 2000); //最多监听10000件事，最多等呆2s
        for(int i = 0; i < fds; i++){//遍历监听事件 
            int sockfd = events[i].data.fd;
            if(sockfd == pipeFd && events[i].events & EPOLLIN){//接收到信号通知
                int sig;
                char signals[1024];
                ret = recv(pipeFd, signals, sizeof(signals), 0);
                if(ret == -1){
                    continue;
                }
                else if(ret == 0){
                    continue;
                }
                else{
                    for(int i = 0; i < ret; ++i){ //遍历通知接收到的信号
                        printf("Caugh the signal %d\n", signals[i]);
                        switch(signals[i]){
                            case SIGALRM: success = true;  //时钟信号关闭统计，说明测试成功结束
                            case SIGTERM: //关闭信号直接stop
                            case SIGINT:{
                                stop = true;
                                break;
                            }
                            default: continue; //其它信号继续
                        }
                    }
                }
            }
            if(events[i].events & EPOLLIN){  //输入事件
                int readBytes = 0; 
                while(true){ //循环把数据全部读出来
                    readBytes = recv(sockfd, buffer, MSG_LEN, 0);//读长度为MSG_LEN的数据，因为数据长度固定为MSG_LEN
                    if (readBytes == -1){ //读取数据出错 
                        if(errno == EAGAIN){ //非阻塞且无数据可读
                            break;
                        }else{ //其它错误
                            closeConn(epollFd, sockfd);
                            break;
                        } 
                    }
                    else if (readBytes == 0){  //服务器已关闭 
                        closeConn(epollFd, sockfd);
                        break;
                    }else{ //成功读取数据
                        printf("read in %d bytes from socket %d with content: %s\n", readBytes, sockfd, buffer);            
                        if(sockfd == sendSocket && strcmp(buffer, successRequestBuff) == 0){ //读到请求成功返回信息 
                            successRequestNum++;//成功请求数加1
                            isRequesting= false;//请求处理成功，结束请求阶段
                        }
                    } 
                }  
            }
            if(events[i].events & EPOLLOUT){//输出就绪
                char tmp[MSG_LEN];
                memset(tmp, '\0', MSG_LEN);//清零
                sprintf(tmp, "Client %d Say : Hello", sockfd);
                if (! writeOnce(sockfd, tmp, MSG_LEN)){ //发送数据长度固定为MSG_LEN的数据，不够长的数据后面补‘\0’；
                    closeConn(epollFd, sockfd);//写错误，关闭测试
                }
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP;//输出数据结束后更改为监听输入事件
                event.data.fd = sockfd;
                epoll_ctl(epollFd, EPOLL_CTL_MOD, sockfd, &event);
            }
            if(events[i].events & EPOLLERR){//错误事件
                closeConn(epollFd, sockfd);//关闭socket，关闭测试
            }
            if(events[i].events & EPOLLRDHUP){//连接被关闭
                closeConn(epollFd, sockfd);//关闭socket，关闭测试
                printf("server close the connection\n"); 
            }
            
        }
    }
    if(success)//测试成功，输出成功请求的数量及测试时间
        std::cout << "successRequestNum: " << successRequestNum << " test time:" <<  TEST_TIME << "s" << std::endl; 
    else//测试失败
        std::cout << "There are some error" << std::endl; 
    closeAllConn(epollFd);//关闭
    close(epollFd); 
}


