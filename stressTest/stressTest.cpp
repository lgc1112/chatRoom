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

#define SERVER_PORT 12345
#define SERVER_IP "0.0.0.0"
#define TEST_NUM 10
#define TEST_TIME 10 //

class stressTest{
public:
    stressTest(int testNum, const char* serverIp, int serverPort) : testNum(testNum), serverPort(serverPort){
        strcpy(this->serverIp, serverIp);
        testFds = new std::vector<int>(testNum);
        for(int i = 0; i < testNum; i++){ //默认设置为-1
            (*testFds)[i] = -1;
            
        }
    }
    ~stressTest(){        
        delete testFds;
    }
    void startTest();
private:
    static bool stop;
    int testNum;
    int serverPort;
    char serverIp[100]; 
    std::vector<int>* testFds;
    std::unordered_map<int, int> fd2IdxMap;

    int setnonblocking(int fd);
    void addfd(int epoll_fd, int fd);
    bool write_nbytes(int sockfd, const char* buffer, int len);
    bool read_once(int sockfd, char* buffer, int len);
    int start_conn(int epoll_fd);
    void close_conn(int epoll_fd, int sockfd);
    void closeAllConn(int epoll_fd);
    static void sig_handler( int sig );
    void addsig( int sig );
};
bool stressTest::stop = false;
void stressTest::closeAllConn(int epoll_fd)
{
    for(int i = 0; i < testNum; i++){ //关闭所有未关闭的连接
        if((*testFds)[i] != -1){
            close_conn(epoll_fd, (*testFds)[i]);
        }
    }
 }
int stressTest::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void stressTest::addfd(int epoll_fd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLERR;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

bool stressTest::write_nbytes(int sockfd, const char* buffer, int len)
{
    int bytes_write = 0;
    printf("write out %d bytes to socket %d\n", len, sockfd);
    while(1) 
    {   
        bytes_write = send(sockfd, buffer, len, 0);
        if (bytes_write == -1)
        {   
            return false;
        }   
        else if (bytes_write == 0) 
        {   
            return false;
        }   

        len -= bytes_write;
        buffer = buffer + bytes_write;
        if (len <= 0) 
        {   
            return true;
        }   
    }   
}

bool stressTest::read_once(int sockfd, char* buffer, int len)
{
    int bytes_read = 0;
    memset(buffer, '\0', len);
    bytes_read = recv(sockfd, buffer, len, 0);
    if (bytes_read == -1) //读取数据出错
    {
        return false;
    }
    else if (bytes_read == 0)  //服务器已关闭
    {
        return false;
    }
	printf("read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer);

    return true;
}

int stressTest::start_conn(int epoll_fd)
{
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, serverIp, &address.sin_addr);
    address.sin_port = htons(serverPort);

    for (int i = 0; i < testNum; ++i)
    {
        usleep(100000);
        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        printf("create 1 sock\n");
        if(sockfd < 0)
        {
            ret = -1;
            continue;
        }

        if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) == 0)
        {
            printf("build connection %d\n", i);
            addfd(epoll_fd, sockfd);
            (*testFds)[i] = sockfd; //保存到测试列表
            fd2IdxMap[sockfd] = i; //建立fd 2 idx 映射关系
        }
    }
    return ret;
}

void stressTest::close_conn(int epoll_fd, int sockfd)
{
    if(fd2IdxMap.count(sockfd) == 1){ //目标fd在连接数组中，置为-1
        (*testFds)[fd2IdxMap[sockfd]] = -1;
        //fd2IdxMap.erase(sockfd);
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);
}

void stressTest::sig_handler( int sig )
{
     stressTest::stop = true;
}

void stressTest::addsig( int sig )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void stressTest::startTest()
{
    int epoll_fd = epoll_create(100);
    int ret = start_conn(epoll_fd);
    if(ret == -1){
        std::cout << "Cannot create " << TEST_NUM << "client" << std::endl; 
    }
    epoll_event events[ 10000 ];
    char buffer[ 2048 ];
    long long successRequestNum = 0;
    bool successRequest = true;
    int sendIdx = -1, sendSocket = 0;  
    addsig( SIGALRM );
    addsig( SIGTERM );
    alarm( TEST_TIME );
    while (!stop)
    {
        if(successRequest){
            successRequest = false;
            sendIdx++;
            sendIdx %= testNum;
            sendSocket = (*testFds)[sendIdx];
            struct epoll_event event;
            event.events = EPOLLOUT | EPOLLET | EPOLLERR;
            event.data.fd = sendSocket;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sendSocket, &event);
        }
        int fds = epoll_wait(epoll_fd, events, 10000, 2000); //最多监听10000件事，最多等呆2s
        for (int i = 0; i < fds; i++)
        {   
            int sockfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {   
                int readBytes = 0;
                memset(buffer, '\0', 2048);
                readBytes = recv(sockfd, buffer, 2048, 0);
                if (readBytes == -1) //读取数据出错
                {
                    close_conn(epoll_fd, sockfd);
                }
                else if (readBytes == 0)  //服务器已关闭
                {
                    close_conn(epoll_fd, sockfd);
                }else{ //成功读取数据
                    printf("read in %d bytes from socket %d with content: %s\n", readBytes, sockfd, buffer);
                    if(sockfd == sendSocket){
                        successRequestNum++;
                        successRequest = true;
                    }
                } 
                // struct epoll_event event;
                // event.events = EPOLLOUT | EPOLLET | EPOLLERR;
                // event.data.fd = sockfd;
                // epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
            }
            else if(events[i].events & EPOLLOUT) 
            {
                char tmp[128];
                sprintf(tmp, "Client %d Say : Hello", sockfd);
                if (! write_nbytes(sockfd, tmp, strlen(tmp)))
                {
                    close_conn(epoll_fd, sockfd);
                }
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
            }
            else if(events[i].events & EPOLLERR)
            {
                close_conn(epoll_fd, sockfd);
            }
        }
    }
    std::cout << "successRequestNum: " << successRequestNum << " client" << std::endl; 
    closeAllConn(epoll_fd);
    close(epoll_fd); 
}

int main(void)
{
    stressTest test(TEST_NUM, SERVER_IP, SERVER_PORT);
    test.startTest();   
}

