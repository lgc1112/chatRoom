#include "myServer.h" 


void MyServer::loadRecords(HistoryRecord* mHistoryRecord)
{
    std::ifstream fin; 
	fin.open(FNAME, std::ios::in | std::ios::binary); //二进制文件打开
    if (!fin)//文件不存在
	{ 
        printf("file does not exist\n");
		//创建文件
		std::ofstream fout(FNAME, std::ios::out | std::ios::binary); //二进制文件形式打开创建该文件
		if (fout)
		{
			// 如果创建成功
			printf("creat file: %s\n" , FNAME);
			// 执行完操作后关闭文件句柄
            fout.write((char*)mHistoryRecord, sizeof(*mHistoryRecord));//直接把内存数据以二进制文件形式保存到文件
            fout.close(); 
			fout.close();
		}
	}
	else//文件存在，加载
	{
        printf("file exists, load into memory\n");
        fin.read((char*)mHistoryRecord, sizeof(*mHistoryRecord)); 
        printf("size: %d, sendIndex: %d\n", mHistoryRecord->getSize(), mHistoryRecord->getSendIdx());

	}
    fin.close();
}

void MyServer::storeRecords(HistoryRecord* mHistoryRecord)
{ 
    std::ofstream fout(FNAME, std::ios::out | std::ios::binary);
    fout.write((char*)mHistoryRecord, sizeof(*mHistoryRecord));//直接把内存数据以二进制文件形式保存到文件
    fout.close();   
}


int MyServer::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}


int MyServer::addClientFd(int epollFd, int fd)
{
    ClientData* tmp = new ClientData; 
    tmp->fd = fd;
    clientfds[userCounter] = tmp;  //保存新fd到用户连接clientfds数组
    fd2IdxMap[fd] = userCounter; //建立fd到clientfds数组idx的映射关系
    userCounter++;
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    event.data.fd = fd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);//注册监听输入等事件
    setnonblocking(fd);
}

int MyServer::addPipeFd(int epollFd, int fd)
{ 
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);//注册监听输入等事件
    setnonblocking(fd);
}

int MyServer::deleteClientFd(int epollFd, int sockfd)
{
    userCounter--; 
    if(sendNum >= userCounter)//如果一个用户退出了，并且发送的次数大于等于剩下的用户数，则需要把正在发送状态置false
        isSending = false;
    int idx = fd2IdxMap[sockfd]; //要删除的fd在clientfds数组对应的对应的idex
    delete clientfds[idx]; //删除该fd对应的ClientData
    clientfds[idx] = clientfds[userCounter];  //将clientfds数组中的最后一个有效clientdata移动到删除的idx
    fd2IdxMap[clientfds[userCounter]->fd] = idx; //更新最后一个有效clientdata对应fd的idx
    fd2IdxMap.erase(sockfd); //该fd已删除，删除映射关系
    epoll_ctl(epollFd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);

}


int MyServer::deleteAllClientFd(int epollFd)
{
    for(int i = 0; i < userCounter; i++){
        close(clientfds[i]->fd);
        fd2IdxMap.erase(clientfds[i]->fd); //该fd已删除，删除映射关系
        epoll_ctl(epollFd, EPOLL_CTL_DEL, clientfds[i]->fd, 0);
        delete clientfds[i]; //删除该fd对应的ClientData
    }
    userCounter = 0; 
}

void MyServer::sendHistoryRecord(int fd)
{
    int size = mHistoryRecord->getSize();
    if(size == 0)
        return;
    if(mHistoryRecord->getSize() < RECORD_SIZE){
        send(fd, mHistoryRecord->records, MSG_LEN * size, 0); //一次性发送完所有历史记录（阻塞模式）
    }else{
        int newestIdx = mHistoryRecord->getSendIdx();//获得当前发送数据所在的idx(最新数据)
        int oldestIdx = newestIdx + 1; //获得当前发送数据的下一条数据(最老数据)
        if(oldestIdx == RECORD_SIZE){//当前发送数据的idx为最后一条，则最旧的数据为第0条
             send(fd, mHistoryRecord->records, MSG_LEN * size, 0); //一次性发送完所有历史记录（阻塞模式）
        }else{//最旧的数据为第idx条
            send(fd, mHistoryRecord->records + oldestIdx * MSG_LEN, MSG_LEN * (RECORD_SIZE - oldestIdx), 0); //一次性发送完最老一条数据到数组最后一条数据（阻塞模式）
            send(fd, mHistoryRecord->records, MSG_LEN * oldestIdx, 0); //一次性发送完数据第0条数据到最新的数据（阻塞模式）
        }
    }
}

void MyServer::startListening()
{
    char successRequestBuff[MSG_LEN] = "send OK"; //请求成功的返回值
    int ret = 0;

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, listenIp, &address.sin_addr);
    address.sin_port = htons(listenPort);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0); //创建TCP流socket
    assert(listenfd >= 0);

    int lowat = MSG_LEN; //设置发送和接收的低水位值，都是MSG_LEN，只有发送或接受缓冲区大于MSG_LEN才触发读写事件，accept返回的sock将会自动继承这个水位值
    setsockopt(listenfd, SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat));
    setsockopt(listenfd, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
    
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address)); //命名socket
    assert(ret != -1);

    ret = listen(listenfd, 5); //开始监听
    assert(ret != -1);
    std::cout << "listening... " << std::endl; 
    
    int epollFd = epoll_create(100);
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLERR;
    event.data.fd = listenfd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, listenfd, &event); //添加listenfd到epoll
    
    addPipeFd(epollFd, pipeFd);  //添加信号通知管道pipeFd到epoll

    epoll_event events[ 1000 ]; //用于epoll存储返回就绪的事件
    while(!stop){
        int fds = epoll_wait(epollFd, events, 1000, 2000); //最多1000件就绪事件，最多等呆2s
        for (int i = 0; i < fds; i++) //遍历就绪事件
        {   
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd && events[i].events & EPOLLIN) //新连接
            { 
                if(isSending) //数据正在分发给各个客户端，需要等待数据发送结束再处理新接收新数据
                    continue;
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength); //接收该连接
                
                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if(userCounter >= USER_LIMIT) //连接数量过多
                {
                    const char info[MSG_LEN] = "Excessive number of users， byebye\n";
                    printf("%s", info);
                    send(connfd, info, MSG_LEN, 0); //告诉对方连接过多
                    close(connfd);//断开该连接
                    continue;
                }
                sendHistoryRecord(connfd);//新连接，发送最近100条历史记录
                addClientFd(epollFd, connfd); //添加新客户fd到epoll并保存新fd到用户连接clientfds数组

                printf("comes a new user, now have %d users\n", userCounter);
                
            }
            else if(sockfd == pipeFd && events[i].events & EPOLLIN){//接收到信号
                int sig;
                char signals[1024];
                ret = recv( pipeFd, signals, sizeof( signals ), 0 );
                if( ret == -1 )
                {
                    continue;
                }
                else if( ret == 0 )
                {
                    continue;
                }
                else
                {
                    for( int i = 0; i < ret; ++i )//遍历接收到的信号
                    { 
                        printf( "Caugh the signal %d\n", signals[i] );
                        switch( signals[i] )
                        {                        
                            case SIGTERM: 
                            case SIGINT: {
                                stop = true;//接收到关闭信号，关闭server
                                break;
                            }
                            default: continue; //其它信号继续
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLERR)//错误事件
            {
                printf("get an error from %d\n", sockfd);
                deleteClientFd(epollFd, sockfd);//直接关闭连接
                char errors[ 100 ];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(errors);
                if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0)
                {
                    printf("get socket option failed\n");
                }
                continue;
            }
            else if(events[i].events & EPOLLRDHUP)//TCP连接被对方关闭事件
            {
                deleteClientFd(epollFd, sockfd); //直接关闭连接
                printf("a client left\n");
            }
            else if(events[i].events & EPOLLIN)//输入事件
            {   
                
                if(isSending) //还有数据正在分发给各个客户端，需要等待数据分发结束再处理新接收的数据
                    continue;              
                int idx = fd2IdxMap[sockfd];//该fd在clientfds数组对应的对应的idex
                ClientData* data = clientfds[idx]; //获得fd对应的用户数据
                char tmpBuff[ MSG_LEN ]; //临时缓存接收到的信息
                memset(tmpBuff, '\0', MSG_LEN);
                ret = recv(sockfd, tmpBuff, MSG_LEN, 0); //接收长度为MSG_LEN的信息
                printf("get %d bytes of client data %s from %d\n", ret, tmpBuff, sockfd); //先读取到临时buff，数据有效再写到历史记录表
                if(ret < 0)
                {
                    if(errno != EAGAIN) //除了无数据可读的错误外，其它错误直接关闭fd
                    {
                        deleteClientFd(epollFd, sockfd);
                        printf("error & delete a client\n"); 
                    }
                }
                else if(ret == 0)//连接已关闭
                {
                    deleteClientFd(epollFd, sockfd);
                    printf("code should not come to here\n");
                }
                else //成功读取到数据
                {
                    //保存数据到历史记录表
                    int index = mHistoryRecord->increaseSendIdx(); //发送index位置加1，返回新的index位置 
                    char* sendPoint = mHistoryRecord->records + index * MSG_LEN;//获得新的发送数据的地址指针
                    memcpy(sendPoint, tmpBuff, MSG_LEN);//复制新数据到mHistoryRecord->records中发送数据的地址
                    for(int j = 0; j < userCounter; j++) //遍历用户fd表clientfds数组
                    {
                        ClientData* tmp = clientfds[j];
                        int fd = tmp->fd;

                        if(fd == sockfd)
                        {
                            tmp->writeBuf = successRequestBuff;                 
                        }else{
                            tmp->writeBuf = sendPoint; //保存发送数据指针到每个客户的待写指针 
                        }
                                               
                        struct epoll_event event;
                        event.events = EPOLLOUT | EPOLLRDHUP | EPOLLERR;//更改epoll监听事件为输出缓冲区可写
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
                if(sendNum >= userCounter) //已发送userCounter次，则发送结束，将isSending状态置false；
                    isSending = false;
                int idx = fd2IdxMap[sockfd];//该fd在clientfds数组对应的对应的idex
                ClientData* data = clientfds[idx];
                if(! data->writeBuf)
                {
                    printf("writeBuf is NULL\n");
                    continue;
                }
                ret = send(sockfd, data->writeBuf, MSG_LEN, 0);//将待写指针的数据发送给客户
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;//发送成功后更改为监听读事件
                event.data.fd = sockfd;
                epoll_ctl(epollFd, EPOLL_CTL_MOD, sockfd, &event); 
            }
        }
    } 
    printf("close\n");
    close(listenfd);//关闭fd
    deleteAllClientFd(epollFd);
}
