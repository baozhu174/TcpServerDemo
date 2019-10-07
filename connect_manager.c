#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#include "terminal.h"
#include "connect_manager.h"
#include "destroy_manager.h"
#include "kernel_list.h"



#define TEST_SEND_INTERVAL 5
#define HEARTBEAT_INTERVAL 100
#define LOOP_INTERVAL   5
#define SEND_THREAD_SUM 5
struct _t_ConnectClient
{
    struct _t_Terminal **tClient;
    struct list_head list;
};

static LIST_HEAD(head_connect);     //所有已经连接的客户端（获取到客户端的MAC）
pthread_mutex_t mutex_connect;



void HandleConnectThreadfunc(void *arg);
void TestSendThread();



struct _t_ConnectClient *Create_ConnectObj(struct _t_Terminal **tClient)
{
    struct _t_ConnectClient *cli = (struct _t_ConnectClient *)malloc(sizeof(struct _t_ConnectClient));
    if (NULL == cli)
    {
        printf("err malloc(%lu)", sizeof(struct _t_ConnectClient));
        return NULL;
    }
    cli->tClient      = tClient;
    INIT_LIST_HEAD(&(cli->list));
    return cli;
}

void Add_ConnectObj_To_Tail(struct _t_ConnectClient        *tCli)
{
	pthread_mutex_lock(&mutex_connect);

	INIT_LIST_HEAD(&(tCli->list));
	list_add_tail(&(tCli->list), &head_connect);
	//printf("[%s][%s:%d] | Thread:%ld | fd: %d | Terminal: %p | Terminal_Ptr: %p\n",  __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), (*tCli->tClient)->fd, *(tCli->tClient), tCli->tClient);
	
	pthread_mutex_unlock(&mutex_connect);

}

void Add_Terminal_To_Tail(struct _t_Terminal        **tClient)
{
	
	struct _t_ConnectClient *cli = Create_ConnectObj(tClient);
	if (NULL == cli)
	{
		Destory_TcpTerminalObj(tClient);
		return;
	}
	
    Add_ConnectObj_To_Tail(cli);
	return;
}

int Is_ConnectList_Empty()
{
    return list_empty(&head_connect);
}

void InitConnectListManager()
{
    InitDestroyManager();
    pthread_mutex_init(&mutex_connect, NULL);

	pthread_t connect_thread_tid;

	int ret;
   
    ret = pthread_create(&(connect_thread_tid) , NULL, (void *)&HandleConnectThreadfunc, NULL);
    if (-1 == ret)
    {
        return;
    }
    ret = pthread_detach(connect_thread_tid);
    if (-1 == ret)
    {
        return;
    }
    printf("[%s][%s:%d]\n", __FILE__, __FUNCTION__, __LINE__);
    return;
}

int AddTerminalToConnectList_Direct(struct _t_Terminal          **tCli)
{
	if (NULL == tCli)
        return -1;

	Add_Terminal_To_Tail(tCli);

	return (*tCli)->fd;
}


int AddTerminalFDToConnectList(int fd, int epoll_fd)
{
	if (-1 == fd || -1 == epoll_fd)
		return -1;
	
	int retFD;

	struct _t_Terminal **tTerminal = Create_TcpTerminalObj(epoll_fd, fd);
	if (NULL == tTerminal)
		return -1;

	if ( -1 == Register_TcpTerminal(tTerminal))
	{
		Destory_TcpTerminalObj(tTerminal);
		return -1;
	}
	
	retFD = AddTerminalToConnectList_Direct(tTerminal);

	return retFD;
}


int SendCommandToTerminalByFd(int fd, uint8_t *data, size_t datasize)
{
	int ret;
		
    ret = SocketSend(fd, data, datasize);

    return ret;
}


int SendCommandToAllTerminals(uint8_t *data, size_t datasize)
{
    struct list_head *cur = NULL;
    struct list_head *tmp = NULL;
    struct _t_ConnectClient *ptr = NULL;

    int node_sum = 0;
    int send_sum = 0;
    int send_success_sum = 0;
	int node_nulldata_sum = 0;
	pthread_mutex_lock(&mutex_connect);

	clock_t start_clock = clock();
    list_for_each_safe(cur, tmp, &head_connect)
    {
        ++node_sum;
        ptr = container_of(cur, struct _t_ConnectClient, list);
		if (NULL != *(ptr->tClient))
		{
            ++send_sum;
            int ret = Send_TcpTerminal(*(ptr->tClient), data, datasize);
			if (ret < 0)
				Close_TcpTerminal(ptr->tClient);
            else
                ++send_success_sum;
		}
		else
		{
			++node_nulldata_sum;
		}
    }
	clock_t end_clock = clock();
	
	pthread_mutex_unlock(&mutex_connect);

	//printf("[%s][%s:%d] | Thread: %ld | all_node: %d | node_nulldata: %d | send_node: %d, success: %d | TimeConsuming: %f\n",  __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), node_sum, node_nulldata_sum, send_sum, send_success_sum, (double)(end_clock - start_clock) / CLOCKS_PER_SEC);

    return 0;
}

void HandleConnectThreadfunc(void *arg)
{
	printf("[%s][%s:%d] | Thread : %ld\n",  __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid));
	while (1)
	{
		sleep(LOOP_INTERVAL);

        struct _t_ConnectClient *ptr  = NULL;
		struct _t_ConnectClient *next = NULL;

        int online_sum = 0;
        int free_sum = 0;
		int timeout = 0;
		pthread_mutex_lock(&mutex_connect);

		clock_t start_clock = clock();
        list_for_each_entry_safe(ptr, next, &head_connect, list)
    	{
            ++online_sum;
            
	        time_t now = time(NULL);
			struct _t_Terminal **pptClient = ptr->tClient;

			if (NULL != *pptClient)
			{
				time_t hb = now - (*pptClient)->last_hb;
				if (HEARTBEAT_INTERVAL < hb)
				{
					//printf("[%s][%s:%d] | fd: %d, Ptr: %p | TimeOut(%ld s), Close\n", __FILE__, __FUNCTION__, __LINE__, (*pptClient)->fd, (*pptClient), hb);
					Close_TcpTerminal(pptClient);
					++timeout;
				}
				if (-1 == (*pptClient)->fd)
				{
					list_del(&ptr->list);
			        free(ptr);
			        ptr = NULL;
			        //printf("[%s][%s:%d] | AddToDestroyList | fd: %d | Terminal: %p | Terminal_Ptr: %p | recv_event=%d\n", __FILE__, __FUNCTION__, __LINE__, (*pptClient)->fd, *pptClient, pptClient, (*pptClient)->recv_event);
					AddToDestroyManagerList(pptClient);
	                ++free_sum;
	                --online_sum;
				}
			}

			/*
			if (NULL != ppTest_ptr)
				if ( (*ppTest_ptr)->fd == (*pptClient)->fd )
					printf("[%s][%s:%d] | test_ptr = %p, %p\n", __FILE__, __FUNCTION__, __LINE__, ppTest_ptr, pptClient);
			*/
				
			
		}
		clock_t end_clock = clock();

		pthread_mutex_unlock(&mutex_connect);

        printf("[%s][%s:%d] | Connected: %d | Timeout: %d | ToFree: %d | TimeConsuming: %f\n", __FILE__, __FUNCTION__, __LINE__, online_sum, timeout, free_sum, (double)(end_clock - start_clock) / CLOCKS_PER_SEC);
	}
}

void TestSendThread()
{
	printf("[%s][%s:%d] | Thread : %ld\n",  __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid));
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0)
    {
        printf("[%s][%s:%d] | block sigpipe error\n", __FILE__, __FUNCTION__, __LINE__);
    }

    while ( 1 )
    {
        sleep(TEST_SEND_INTERVAL);
        char buf[8] = {1};
        SendCommandToAllTerminals((uint8_t*)buf, 8);
    }
}

void InitTestSendThread()
{
    pthread_t pid[SEND_THREAD_SUM];

    int i;
    for ( i = 0; i < SEND_THREAD_SUM; i++ )
    {
        int ret;
   
        ret = pthread_create(&(pid[i]) , NULL, (void *)&TestSendThread, NULL);
        if (-1 == ret)
        {
            return;
        }
        ret = pthread_detach(pid[i]);
        if (-1 == ret)
        {
            return;
        }
    }
    printf("[%s][%s:%d]\n", __FILE__, __FUNCTION__, __LINE__);
}
