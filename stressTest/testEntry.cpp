#include "stressTest.h"
#define SERVER_PORT 12345
#define SERVER_IP "0.0.0.0"

#define TEST_NUM 10 //测试的用户数量
static int pipeFd[2];//存储管道fd，主要用于信号通知

void sig_handler(int sig)
{
    int msg = sig;
    send(pipeFd[1], (char*)&msg, 1, 0); //发送信号值到管道
}

void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(void)
{
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipeFd);//建立双向管道
    assert(ret != -1); 
    
    StressTest test(TEST_NUM, SERVER_IP, SERVER_PORT, pipeFd[0]);//创建压力测试实例，传入\
    测试数量，传入监听的ip，端口号，和信号通知管道，创建server实例
    addsig(SIGALRM); //注册时钟信号
    addsig(SIGINT); //注册关闭信号
    addsig(SIGTERM);
    alarm(TEST_TIME); //等待TEST_TIME秒触发信号
    test.startTest();   
    close(pipeFd[1]);
    close(pipeFd[0]);
}
