#include "myServer.h"
 
#define LISTEN_PORT 12345  //监听的端口
#define LISTEN_IP "0.0.0.0" //监听的ip，本机

static int pipeFd[2]; //双向管道，主要用于向mServer发送关闭信号

void sig_handler(int sig)
{
    int msg = sig;
    send(pipeFd[1], (char*)&msg, 1, 0);//向mServer管道发送关闭信号
}

void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);//注册信号 
}

int main(void)
{
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipeFd); //双向管道，主要用于向mServer发送关闭信号
    assert(ret != -1); 

    addsig(SIGINT); //注册关闭信号 
    addsig(SIGTERM);
    const char* ip = LISTEN_IP;
    int port = LISTEN_PORT;
    MyServer mServer(ip, port, pipeFd[0]);//传入监听的ip，端口号，和信号通知管道，创建server实例
    mServer.startListening();//开始监听
    close(pipeFd[1]);//关闭
    close(pipeFd[0]);
    return 0;
}
