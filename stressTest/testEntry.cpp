#include "stressTest.h"
#define SERVER_PORT 12345
#define SERVER_IP "0.0.0.0"

void sig_handler( int sig )
{
    if(sig == SIGALRM)
        stressTest::success = true;//时钟信号关闭统计，说明测试成功结束
    stressTest::stop = true;//关闭统计
}

void addsig( int sig )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

int main(void)
{
    stressTest test(TEST_NUM, SERVER_IP, SERVER_PORT);
    addsig( SIGALRM ); //注册时钟信号
    addsig( SIGINT ); //添加关闭信号，防止关闭后socket资源没有释放
    addsig( SIGTERM );
    alarm( TEST_TIME ); //等待TEST_TIME秒触发
    test.startTest();   
}
