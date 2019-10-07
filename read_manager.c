#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>



#include "read_manager.h"
#include "kernel_list.h"


//#include "hmi.h"
//#include "fpga.h"

//#define MAXBUFFERSIZE  0x100000
//#define READBUFFERSIZE 4096
#define DEBUG_PRINT_READ_MANAGER "/tmp/read_manager.txt"
typedef uint8_t U8;

#define READ_THREADPOOL_SUM         10


struct _t_ReadClient
{
    struct _t_Terminal **tClient;
    struct list_head list;
};
static LIST_HEAD(head_read);     //所有read事件的 终端对象集合
pthread_mutex_t mutex_read;
pthread_cond_t  cond_read;


static int StopRead = 0;


struct _t_ReadClient *Create_ReadObj(struct _t_Terminal **tClient)
{
    struct _t_ReadClient *cli = (struct _t_ReadClient *)malloc(sizeof(struct _t_ReadClient));
    if (NULL == cli)
    {
        printf("err malloc(%lu)\n", sizeof(struct _t_ReadClient));
        return NULL;
    }
    cli->tClient      = tClient;
    INIT_LIST_HEAD(&(cli->list));
    return cli;
}


void Add_ReadObj_To_Tail(struct _t_ReadClient *cli)
{
    pthread_mutex_lock(&mutex_read);

    INIT_LIST_HEAD(&(cli->list));

    //printf("[%s][%s:%d] | Thread:%ld | fd: %d | Terminal: %p | Terminal_Ptr: %p\n", __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid), (*(cli->tClient))->fd, *(cli->tClient), cli->tClient);
    list_add_tail(&(cli->list), &head_read);
	
    pthread_cond_broadcast(&cond_read);
    pthread_mutex_unlock(&mutex_read);	
}

int Is_ReadList_Empty()
{
    return list_empty(&head_read);
}

struct _t_ReadClient *Get_ReadObj_From_Head()
{
    struct list_head      *cur = NULL;
    struct _t_ReadClient  *ptr = NULL;
    struct list_head      *tmp = NULL;
    list_for_each_safe(cur, tmp, &head_read)
    {
        ptr = container_of(cur, struct _t_ReadClient, list);
        return ptr;
    }
    return NULL;
}

struct _t_ReadClient *Remove_ReadObj_From_Head()
{
    struct _t_ReadClient      *ptr = NULL;
    ptr = Get_ReadObj_From_Head();
    if(ptr != NULL)
        list_del_init(&(ptr->list));
    return ptr;
}

void AddToReadManagerList(struct _t_Terminal **tClient)
{
	if (NULL == *tClient)
		return;

	if (0 != SetTcpTerminalRecvEvent(*tClient))
		return;
	
	struct _t_ReadClient *cli = Create_ReadObj(tClient);
    if (NULL != cli)
    {
        Add_ReadObj_To_Tail(cli);
    }
	return;
}

/*****************************************************************************
  Function:     HandleReadThreadfunc
  Description:  接收指令线程
  Input:        none
  Output:       none
  Return:       none
  Author:       yujian.liu
*****************************************************************************/
void HandleReadThreadfunc(void *arg)
{
	printf("[%s][%s:%d] | Thread : %ld\n",  __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid));
    int ret;
    char buffer[IP_INS_LEN_MAX] = {0};
    int readsize = 0;
    int timeoutCount = 0;

    while(StopRead)
    {
        usleep(200000);
        timeoutCount++;
        if(timeoutCount >= 25)
        {
            return;
        }
        continue;
    }

    while(!StopRead)
    {
        pthread_mutex_lock(&mutex_read);
        while ( Is_ReadList_Empty() )
        {
            pthread_cond_wait(&cond_read, &mutex_read);
        }
        struct _t_ReadClient *tCli = Remove_ReadObj_From_Head();
		struct _t_Terminal **tClient = tCli->tClient;
		free(tCli);
        pthread_mutex_unlock(&mutex_read);

		ret = Recv_TcpTerminal(*tClient, (U8*)buffer, &readsize);
		if (-1 == ret)
		{
			Close_TcpTerminal(tClient);
		}
		else
		{
            ret = Send_TcpTerminal(*tClient, (U8*)buffer, readsize);
            if (-1 == ret)
            {
                Close_TcpTerminal(tClient);
            }
        }
        ResetTcpTerminalRecvevent(*tClient);

/*
		ip_ins_rbuffer_write((U8*)buffer, readsize);
		pthread_mutex_lock(&tClient->mutex);
		HandleProcessHeartBeat((uint8_t*)buffer, ret, tClient->fd);
		pthread_mutex_unlock(&tClient->mutex);
*/
    }

    StopRead = 0;
}


void InitReadManager()
{
    int ret;
    pthread_mutex_init(&mutex_read, NULL);
    pthread_cond_init(&cond_read, NULL);

    pthread_t read_thread_tid[READ_THREADPOOL_SUM];

    int i;
    for (i = 0; i < READ_THREADPOOL_SUM; ++i)
    {
        ret = pthread_create(&(read_thread_tid[i]) , NULL, (void *)&HandleReadThreadfunc, NULL);
        if (-1 == ret)
        {
            return;
        }
        ret = pthread_detach(read_thread_tid[i]);
        if (-1 == ret)
        {
            return;
        }
    }

    printf("[%s][%s:%d]\n", __FILE__, __FUNCTION__, __LINE__);
}

void StopReadManager()
{
    StopRead = 1;
    struct _t_ReadClient  *ptr = NULL;
    while(1)
    {
        ptr = Remove_ReadObj_From_Head();
        if(ptr == NULL)
        {
            break;
        }
		free(ptr);
        ptr = NULL;
    }
}

