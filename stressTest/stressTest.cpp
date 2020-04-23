#include "stressTest.h"

void StressTest::closeAllConn()
{
    for(int i = 0; i < testNum; i++){ //关闭所有未关闭的连接
        if(testClientDatas[i]->fd != -1){ //连接是否存在，存在则关闭
            closeConn(testClientDatas[i]->fd);
            testClientDatas[i]->fd == -1;
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

void StressTest::addClientFd(int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void StressTest::addPipeFd(int fd)
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
    PRINT_DATA("write out %d bytes to socket %d\n", len, sockfd);
    bytes_write = send(sockfd, buffer, len, 0);//写数据
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
    bytes_read = recv(sockfd, buffer, len, 0);//读数据，这里传入的长度一般固定为MSG_MAX_LEN
    if (bytes_read == -1){ //读取数据出错
        return false;
    }
    else if (bytes_read == 0){  //服务器已关闭 
        return false;
    }
	printf("read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer);

    return true;
}

int StressTest::startConn()
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
        // int lowat = MSG_MAX_LEN; //设置发送和接收的低水位值，都是MSG_MAX_LEN，只有发送或接受缓冲区大于MSG_MAX_LEN才触发读写事件
        // setsockopt(sockfd, SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat));
        // setsockopt(sockfd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
        printf("create 1 sock\n");
        if(sockfd < 0){//建立连接失败 ret = -1; 
            ret = -1;
            continue;
        }

        if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) == 0){//建立连接 
            printf("build connection %d, socke %d\n", i, sockfd);
            addClientFd(sockfd);  
            //(*testFds)[i] = sockfd; //保存到测试列表 
            clientData* tmp = testClientDatas[i];
            (tmp)->fd = sockfd;//保存到测试列表 
            fd2IdxMap[sockfd] = i; //建立fd 2 idx 映射关系
        }else{
            ret = -1;//建立连接失败 ret = -1;
        }
    }
    return ret;
}

void StressTest::closeConn(int sockfd)
{
    stop = true; //有测试fd被关闭，测试停止
    //if(fd2IdxMap.count(sockfd) == 1){ //目标fd在连接数组中，置为-1
        //(*testFds)[fd2IdxMap[sockfd]] = -1;
    //    
        //fd2IdxMap.erase(sockfd);
    //}
    testClientDatas[fd2IdxMap[sockfd]]->fd = -1;
    epoll_ctl(epollFd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);
}

void StressTest::init()
{
    epollFd = epoll_create(100);
    int ret = startConn();//建立testNum个连接
    if(ret == -1){//无法建立testNum个连接，直接stop
        std::cout << "Cannot create " << testNum << " client" << std::endl; 
        stop = true;
    }
    addPipeFd(pipeFd);  //监听信号通知管道   
}

int StressTest::handleMsg(char* buf, int len, int sockfd){
    if(len < 3)
        return len;
    int checkStart = 0, checkEnd = 0;
    int i = 0;
    for(; i <= len - 4; ){
        if(buf[i] == 0x11 && buf[i + 1] == 0x22 && buf[i + 2] == 0x33){//包头关键字是否对
            int msgLen = buf[i + 3];
            checkStart = i;
            checkEnd = checkStart + 3 + msgLen;//这包数据的最后一个值所在位置
            if(checkEnd < len){ 
                PRINT_DATA("read in %d bytes from socket %d with content: %s\n", msgLen, sockfd, buf + checkStart + 4);             
                i = checkEnd + 1;//下一包数据的起始位置
            }else{ //该包没有接收完整，等待下次接收完整再处理，还有最后len - checkStart个字节未处理
                printf("read next time\n"); 
                memcpy(buf, buf + checkStart, len - checkStart);
                return len - checkStart;
            }
        }else if(buf[i] == 0x11 && buf[i + 1] == 0x22 && buf[i + 2] == 0x22  && buf[i + 3] == 0){//包头关键字是否对,这是ack信息
            PRINT_DATA("read ask from socket %d\n", sockfd); 
            i += 4;
            successRequestNum++;//成功请求数加1
            int idx = fd2IdxMap[sockfd];
            testClientDatas[idx]->isRequesting = false;//请求处理响应成功，结束请求阶段 
        }else{
            printf("read error %d : %x\n", i, buf[i]); 
            i++;//不是包头，出错，试试下一个是不是
        }
    }
    if(i == len){//刚好读完
        //PRINT_DATA("success\n"); 
        return 0;
    }else if(i < len){//还有最后i个字节没有处理
        printf("read error2\n"); 
        memcpy(buf, buf + i, len - i);
        return len - i;
    }
    
}
    
int StressTest::pack(char* dst, char* src)
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

void StressTest::startTest()
{
    epoll_event events[10000];
    char buffer[MSG_MAX_LEN];
    bool isRequesting = false;//是否处于正在请求的阶段
    int sendIdx = -1;//发送请求的fd在testFds中的index 
    int oldSendIdx = -1;
    int sendSocket = 0;  //发送请求的socket fd
    printf("startTest......\n");
    while(!stop){  
        oldSendIdx = sendIdx;
        sendIdx++;//更换下一个fd用于发送请求
        if(sendIdx == testNum)
            sendIdx = 0;           
        clientData* tmp = testClientDatas[sendIdx]; 
        
        if(tmp->isRequesting){//该socket处于发送了请求，但未收到ask阶段， 需等待收到ask才能继续发送下一条请求
            PRINT_DATA("Waiting for %d \n", sendIdx);
            sendIdx = oldSendIdx; //回退
        }else{     
            sendSocket = tmp->fd;//得到下一个发送请求的发送请求的socket fd            
            struct epoll_event event;
            event.events = EPOLLOUT | EPOLLET | EPOLLERR | EPOLLRDHUP;//更改为监听输出事件，准备发送请求数据
            event.data.fd = sendSocket;
            epoll_ctl(epollFd, EPOLL_CTL_MOD, sendSocket, &event);
            tmp->isRequesting = true;//请求阶段开始
        } 

        int fds = epoll_wait(epollFd, events, 10000, 2000); //最多监听10000件事，最多等呆2s
        for(int i = 0; i < fds; i++){//遍历监听事件 
            int sockfd = events[i].data.fd;
            if(sockfd == pipeFd && events[i].events & EPOLLIN){//接收到信号通知
                int sig;
                char signals[1024];
                int ret = recv(pipeFd, signals, sizeof(signals), 0);
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
                int idx = fd2IdxMap[sockfd];
                clientData* data = testClientDatas[idx]; 
                readBytes = recv(sockfd, data->receiveBuf + data->receivedLen, REC_LEN - data->receivedLen, 0);//一次性读很多数据，最多REC_LEN
                if (readBytes == -1){ //读取数据出错 
                    if(errno == EAGAIN){ 
                        continue;
                    }else{ //其它错误
                        closeConn(sockfd);
                        continue;
                    } 
                }
                else if (readBytes == 0){  //服务器已关闭 
                    closeConn(sockfd);
                    continue;
                }
                else{
                    PRINT_DATA("read in %d bytes from socket %d\n", readBytes, sockfd); 
                    data->receivedLen = handleMsg(data->receiveBuf, data->receivedLen + readBytes, sockfd);//处理这些数据
                } 
            }
            if(events[i].events & EPOLLOUT){//输出就绪
                char tmp[MSG_MAX_LEN];
                memset(tmp, '\0', MSG_MAX_LEN);//清零
                sprintf(tmp, "Client %d Say : Hello", sockfd);
                char send[MSG_MAX_LEN]; //封包  
                int len = pack(send, tmp);  
                if (!writeOnce(sockfd, send, send[3] + 4)){ //发送数据
                    closeConn(sockfd);//写错误，关闭测试
                } 
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP;//输出数据结束后更改为监听输入事件
                event.data.fd = sockfd;
                epoll_ctl(epollFd, EPOLL_CTL_MOD, sockfd, &event);
            }
            if(events[i].events & EPOLLERR){//错误事件
                closeConn(sockfd);//关闭socket，关闭测试
            }
            if(events[i].events & EPOLLRDHUP){//连接被关闭
                closeConn(sockfd);//关闭socket，关闭测试
                printf("server close the connection\n"); 
            }
            
        }
    }
    if(success)//测试成功，输出成功请求的数量及测试时间
        std::cout << "successRequestNum: " << successRequestNum << " test time:" <<  TEST_TIME << "s" \
        << " test client number:" <<  testNum << std::endl; 
    else//测试失败
        std::cout << "There are some error" << std::endl; 
    closeAllConn();//关闭
    close(epollFd); 
}


