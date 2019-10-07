#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>

#include "terminal.h"


struct _t_Terminal **ppTest_ptr = NULL;
int test_fd = -1;


ssize_t SocketSend(int sockfd, const uint8_t* buffer, size_t buflen)
{
    ssize_t tmp;
    size_t  total = buflen;
    const uint8_t *p = buffer;
    while ( 1 )
    {
        // Use send with a MSG_NOSIGNAL to ignore a SIGPIPE signal which would cause the process exit.
        tmp = send(sockfd, p, total, MSG_NOSIGNAL | MSG_DONTWAIT);
        if ( tmp < 0 )
        {
            if ( EINTR == errno )
            {
                //printf("[%s][%s:%d] | Thread:%ld | fd: %d, | Send err: %s(error: %d)\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), (int)sockfd, strerror(errno), errno);
                return -1;
            }
            // 当socket是非阻塞时,如返回此错误,表示写缓冲队列已满,
            // 在这里做延时后再重试.
            if ( EAGAIN == errno )
            {
                usleep(1000);
                continue;
            }
            //printf("[%s][%s:%d] | Thread:%ld | fd: %d, | Send err: %s(error: %d)\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), (int)sockfd, strerror(errno), errno);
            return -1;
        }
        if ( (size_t)tmp == total )
        {
        	//printf("Thread:%ld | fd: %d, | Send size: %d\n", syscall(SYS_gettid), (int)sockfd, (int)buflen);
            return buflen;
        }
        total -= tmp;
        p += tmp;
    }
	//printf("Thread:%ld | fd: %d, | Send size: %d\n", syscall(SYS_gettid), sockfd, (int)tmp);
    return tmp;
}

int SetNonBlocking(int sock)
{
    int opts;
    opts = fcntl(sock, F_GETFL, 0);
    if ( opts < 0 )
    {
        perror("fcntl(sock,GETFL)\n");
        return -1;
    }
    opts = opts | O_NONBLOCK;
    if ( fcntl(sock, F_SETFL, opts) < 0 )
    {
        perror("fcntl(sock,SETFL,opts)\n");
        return -1;
    }
    //LOG_printf(LOG_LEVEL_DEBUG, LOG_MODULE_CMD,"Set Socket:%d O_NONBLOCK\n", sock);
    return 0;
}

void Init_TcpTerminal(struct _t_Terminal *cli)
{
	if (NULL == cli)
		return;

	
	cli->epoll_fd  = -1;
    cli->fd        = -1;	
    cli->last_hb   = time(NULL);
	cli->recv_event = 0;	
	pthread_mutex_init(&cli->send_mutex,NULL); 
	pthread_mutex_init(&cli->recv_mutex,NULL); 

	return;
}

struct _t_Terminal **Create_TcpTerminalObj(int epoll_fd, int socket_fd)
{

	struct _t_Terminal *cli = (struct _t_Terminal *)malloc(sizeof(struct _t_Terminal));
    if (NULL == cli)
    {
		printf("[%s][%s:%d] | err malloc(%lu)\n", __FILE__, __FUNCTION__, __LINE__, sizeof(struct _t_Terminal));
        return NULL;
    }
	//printf("sizeof(struct _t_Terminal)=%lu | Ptr : %p\n", sizeof(struct _t_Terminal), cli);

	Init_TcpTerminal(cli);
	cli->epoll_fd = epoll_fd;
	cli->fd		  = socket_fd;

	struct _t_Terminal **ppcli = (struct _t_Terminal **)malloc(sizeof(struct _t_Terminal*));
	*ppcli = cli;

	//printf("[%s][%s:%d] | Thread:%ld | fd: %d | Terminal: %p | Terminal_Ptr: %p\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), (*ppcli)->fd, *ppcli, ppcli);

	return ppcli;
}

void Destory_TcpTerminalObj(struct _t_Terminal **ppCli)
{
	if (NULL == *ppCli)
		return;
	
	// wait for for RecvEvent finished
	//printf("[%s][%s:%d] | Thread:%ld | fd: %d | Terminal: %p | Terminal_Ptr: %p | recv_event=%d\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), (*ppCli)->fd, *ppCli, ppCli, (*ppCli)->recv_event);
	while (1)
	{
		pthread_mutex_lock(&((*ppCli)->recv_mutex));

		if (0 == (*ppCli)->recv_event)
		{
			pthread_mutex_unlock(&((*ppCli)->recv_mutex));
			break;
		}

		pthread_mutex_unlock(&((*ppCli)->recv_mutex));
	}
	pthread_mutex_destroy(&(*ppCli)->send_mutex); 
	pthread_mutex_destroy(&(*ppCli)->recv_mutex); 

    //printf("[%s][%s:%d] | %p\n", __FILE__, __FUNCTION__, __LINE__, *ppCli);
    free(*ppCli);
	*ppCli = NULL;
	free(ppCli);
	ppCli = NULL;
}

int SetTcpTerminalRecvEvent(struct _t_Terminal *cli)
{
	int ret = 0;
	if (NULL == cli)
		return -1;

	pthread_mutex_lock(&cli->recv_mutex);
	/*
	if (0 == cli->recv_event) 
	{
		cli->recv_event = 1;
	}
	else
	{
		ret = -1;
	}
	*/
	++cli->recv_event;
	//printf("[%s][%s:%d] | Thread:%ld | fd: %d | Terminal: %p | recv_event=%d\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), cli->fd, cli, cli->recv_event);
	pthread_mutex_unlock(&cli->recv_mutex);

	return ret;
}

void ResetTcpTerminalRecvevent(struct _t_Terminal *cli)
{
	if (NULL == cli)
		return;

    //printf("[%s][%s:%d] | %p\n", __FILE__, __FUNCTION__, __LINE__, cli);
	//cli->recv_event = 0;
	pthread_mutex_lock(&cli->recv_mutex);
	--cli->recv_event;
	//printf("[%s][%s:%d] | Thread:%ld | fd: %d | Terminal: %p | recv_event=%d\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), cli->fd, cli, cli->recv_event);
	pthread_mutex_unlock(&cli->recv_mutex);

	return;
}

void Close_TcpTerminal(struct _t_Terminal **ppCli)
{
	if (NULL == *ppCli)
		return;

    pthread_mutex_lock(&((*ppCli)->recv_mutex));
    pthread_mutex_lock(&((*ppCli)->send_mutex));


	if (-1 == (*ppCli)->fd)
	{
    	pthread_mutex_unlock(&((*ppCli)->send_mutex));		
		pthread_mutex_unlock(&((*ppCli)->recv_mutex));		
		return;
	}

	int ret;
	if (-1 != (*ppCli)->epoll_fd)
	{
		ret = epoll_ctl((*ppCli)->epoll_fd, EPOLL_CTL_DEL, (*ppCli)->fd, NULL);
        if (0 != ret)
        {
            printf("[%s][%s:%d] | EPOLL_CTL_DEL, fd=%d, Ptr=%p, %s(error: %d)\n", __FILE__, __FUNCTION__, __LINE__, (*ppCli)->fd, (*ppCli), strerror(errno), errno);
        }
        else
        {
        	/*
        	ppTest_ptr = ppCli;
			test_fd  = (*ppCli)->fd;
			*/
            //printf("[%s][%s:%d] | EPOLL_CTL_DEL, fd=%d, Ptr=%p, Test_ptr=%p\n", __FILE__, __FUNCTION__, __LINE__, (*ppCli)->fd, (*ppCli), ppTest_ptr);
        }
	
		(*ppCli)->epoll_fd = -1;
	}
	shutdown((*ppCli)->fd, SHUT_RDWR);
	close((*ppCli)->fd);
	(*ppCli)->fd = -1;

    pthread_mutex_unlock(&((*ppCli)->send_mutex));    
	pthread_mutex_unlock(&((*ppCli)->recv_mutex));	

	return;
}

int Recv_TcpTerminal(struct _t_Terminal *cli, uint8_t *data, int *size)
{
	if (NULL == cli)
		return 0;
/*
    printf("[%s][%s:%d] | %p | sleep(60)\n", __FILE__, __FUNCTION__, __LINE__, cli);
    sleep(60);
    printf("[%s][%s:%d] | %p | sleep(60), wake up\n", __FILE__, __FUNCTION__, __LINE__, cli);
*/   
	int ret;
	*size = 0;

	int result = 0;
	
	pthread_mutex_lock(&cli->recv_mutex);
	
    while ( 1 )
    {
        ret = recv(cli->fd, data, IP_INS_LEN_MAX, 0);
        if ( -1 == ret )
        {
            if ( EAGAIN == errno || EWOULDBLOCK == errno )
            {
              	//printf("Thread:%ld | fd: %d, Ptr: %p | Loop read, Recv size=%d\n", syscall(SYS_gettid), cli->fd, cli, *size);
                break;
            }
            else
            {
                //printf("[%s][%s:%d] | Thread:%ld | fd: %d, Ptr: %p | Recv err: %s(error: %d)\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid),cli->fd, cli, strerror(errno), errno);
                //TODO 关闭客户端
                result = -1;
                break;
            }
        }
        else if ( 0 ==ret )
        {
            //printf("Thread:%ld | fd: %d, Ptr: %p | Client requests to Close\n", syscall(SYS_gettid),cli->fd, cli);
            //TODO 关闭客户端
            result = -1;
            break;
        }
        else
        {
            *size += ret;;
        }
    }
	cli->last_hb    = time(NULL);
	
	pthread_mutex_unlock(&cli->recv_mutex);

	return result;
}

int Send_TcpTerminal(struct _t_Terminal *cli, const uint8_t *data, const int size)
{
	if (NULL == cli || NULL == data || 0 == size)
		return 0;

	int ret = 0;
	pthread_mutex_lock(&cli->send_mutex);
	ret = SocketSend(cli->fd, data, size);
	if (ret < 0)
	{
		//printf("[%s][%s:%d] | fd: %d, Ptr: %p | Send err: %s(error: %d)\n", __FILE__, __FUNCTION__, __LINE__, cli->fd, cli, strerror(errno), errno);
	}
	pthread_mutex_unlock(&cli->send_mutex);

	return ret;
}

int Register_TcpTerminal(struct _t_Terminal        **ppCli)
{
	if (NULL == *ppCli)
		return -1;
	if (-1 == (*ppCli)->fd || -1 == (*ppCli)->epoll_fd)
		return -1;
		
	int fd = (*ppCli)->fd;
    if ( SetNonBlocking(fd) )
    {
    	printf("[%s][%s:%d] | SetNonBlocking Error |fd=%d, Terminal:%p, Terminal_Ptr:%p\n", __FILE__, __FUNCTION__, __LINE__, fd, (*ppCli), ppCli);
    	close(fd);
		return -1;
    }
	
    struct epoll_event ev;
    ev.data.ptr = ppCli;
    ev.events   = EPOLLIN | EPOLLET;
	
    if ( -1 == epoll_ctl((*ppCli)->epoll_fd, EPOLL_CTL_ADD, fd, &ev) )
    {
        printf("[%s][%s:%d] | EPOLL_CTL_ADD Error | Terminal fd=%d, Ptr=%p PPtr=%p\n", __FILE__, __FUNCTION__, __LINE__, fd, (*ppCli), ppCli);
        close(fd);
		return -1;
    }
    //printf("[%s][%s:%d] | EPOLL_CTL_ADD Success | fd=%d, Terminal:%p, Terminal_Ptr:%p\n", __FILE__, __FUNCTION__, __LINE__, fd, (*ppCli), ppCli);

    return fd;
}



	
