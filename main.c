#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/syscall.h>


#include "terminal.h"
#include "connect_manager.h"
#include "read_manager.h"

#define EVENTS_SUM   1024
#define DEFAULT_PORT 8082
#define DEFAULT_IP   "127.0.0.1"
#define RECV_BUF     1024
#define RECV_TMP_BUF 1024




int main(int argc, char *argv[])
{
	printf("[%s][%s:%d] | Thread : %ld\n",  __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid));
    InitConnectListManager();
    InitReadManager();
    InitTestSendThread();
    printf("EPOLLIN:%d, EPOLLOUT:%d, EPOLLRDHUP:%d, EPOLLPRI:%d, EPOLLERR:%d\n", EPOLLIN, EPOLLOUT, EPOLLRDHUP, EPOLLPRI, EPOLLERR);
    int ep_fd;
    struct epoll_event events[EVENTS_SUM];
    struct epoll_event ev;
    int nfds;
    int i;
    int    server_sock_fd, client_sock_fd, sock_fd;
    struct sockaddr_in     servaddr;

    //初始化Socket
    if( (server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    //初始化
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //servaddr.sin_addr.s_addr = inet_addr(DEFAULT_IP);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。
    servaddr.sin_port = htons(DEFAULT_PORT);//设置的端口为DEFAULT_PORT

    if ( SetNonBlocking(server_sock_fd) )
    {
        exit(0);
    }

    //将本地地址绑定到所创建的套接字上
    if( bind(server_sock_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
    {
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    printf("%-20s\t%s:%d\n", "Server:", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

    //开始监听是否有客户端连接
    if( listen(server_sock_fd, SOMAXCONN) == -1)
    {
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        goto close_server;
    }
    printf("======waiting for client's request======\n");  

    ep_fd = epoll_create(EVENTS_SUM);

    struct _t_Terminal **pptServer = Create_TcpTerminalObj(ep_fd, server_sock_fd);
    if ( NULL == *pptServer )
    {
        goto close_server;
    }
    ev.data.ptr  = pptServer;
    ev.events   = EPOLLIN | EPOLLET;//边缘触发
    if ( -1 == epoll_ctl(ep_fd, EPOLL_CTL_ADD, server_sock_fd, &ev) )
    {
        printf("epoll_ctl err :%s(errno: %d)", strerror(errno), errno);
        goto free_server;
    }
    
    printf("EPOLL_CTL_ADD | server_fd:%d epoll_fd:%d ev.data.ptr:%p| EPOLLIN | EPOLLET\n", (*pptServer)->fd, ep_fd, ev.data.ptr);

    while ( 1 )
    {
        nfds = epoll_wait(ep_fd, events, EVENTS_SUM, 500);
        if ( nfds <= 0 )
        {
            continue;
        }

        //printf("epoll_wait nfds = %d\n", nfds);
        for ( i = 0; i < nfds; i++ )
        {
        	//printf("epoll_wait | Server ev.data.ptr: %p, fd: %d\n", events[i].data.ptr, (*((struct _t_Terminal **)(events[i].data.ptr)))->fd);
            if ( (*((struct _t_Terminal **)(events[i].data.ptr)))->fd == server_sock_fd )
            {
                while ( 1 )
                {
                    struct sockaddr_in clientaddr;
                    memset(&clientaddr, 0, sizeof(clientaddr));
                    socklen_t clilen;
                    clilen = sizeof(struct sockaddr);

                    client_sock_fd = accept(server_sock_fd, (struct sockaddr*)&clientaddr, &clilen);
                    if ( client_sock_fd < 0 )
                    {
                        break;
                    }
                    //printf("%-20s socket_fd = %-6d %s:%d \n", "Client accept:", client_sock_fd, inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
                    AddTerminalFDToConnectList(client_sock_fd, ep_fd);
                    
                    continue;  
                }
            }
			
            else if ( events[i].events & EPOLLIN )
            {
                if ( (sock_fd = (*((struct _t_Terminal **)events[i].data.ptr))->fd) < 0 )
                {
                    continue;
                }           
                struct _t_Terminal **ppcli = (struct _t_Terminal **)events[i].data.ptr;
				//printf("[%s][%s:%d] | Thread:%ld | fd: %d | Terminal: %p | Terminal_Ptr: %p\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), (*ppcli)->fd, *ppcli, ppcli);
                AddToReadManagerList(ppcli);
            }
            
        }
        
    }
    printf("quit\n");

free_server:
    Destory_TcpTerminalObj(pptServer);
    
close_server:
    if ( -1 != server_sock_fd )
    {
        close(server_sock_fd);
        server_sock_fd = -1;
    }
    
    return 0;
}
