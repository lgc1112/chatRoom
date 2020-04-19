#include "stressTest.h"
#define SERVER_PORT 12345
#define SERVER_IP "0.0.0.0"

static int pipeFd[2];

void sig_handler( int sig )
{
    int msg = sig;
    send( pipeFd[1], ( char* )&msg, 1, 0 ); 
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
    int ret = socketpair( PF_UNIX, SOCK_STREAM, 0, pipeFd );
    assert( ret != -1 ); 
    
    stressTest test(TEST_NUM, SERVER_IP, SERVER_PORT, pipeFd[0]);
    addsig( SIGALRM ); //注册时钟信号
    addsig( SIGINT ); //添加关闭信号，防止关闭后socket资源没有释放
    addsig( SIGTERM );
    alarm( TEST_TIME ); //等待TEST_TIME秒触发
    test.startTest();   
    close( pipeFd[1] );
    close( pipeFd[0] );
}
