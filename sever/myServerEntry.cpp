
#include "myServer.h"

void sig_handler( int sig )
{
    MyServer::stop = true;//关闭统计
    printf("stop\n");
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
    addsig( SIGINT ); //添加关闭信号 
    addsig( SIGTERM );
    const char* ip = LISTEN_IP;
    int port = LISTEN_PORT;
    MyServer mServer(ip, port);
    mServer.startListening();
    return 0;
}
