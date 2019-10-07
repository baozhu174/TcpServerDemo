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



#include "destroy_manager.h"
#include "kernel_list.h"

#define DESTROY_THREADPOOL_SUM 1
#define DESTROY_INTERVAL_MILLISECOND 5000


struct _t_DestroyList
{
    struct _t_Terminal **tClient;
    struct list_head list;
};

static LIST_HEAD(head_destroy);
pthread_mutex_t mutex_destroy;
pthread_cond_t  cond_destroy;


static int StopDestroy = 0;


struct _t_DestroyList* Create_DestroyObj(struct _t_Terminal **tClient)
{
    struct _t_DestroyList *cli = (struct _t_DestroyList *)malloc(sizeof(struct _t_DestroyList));
    if (NULL == cli)
    {
        printf("err malloc(%lu)\n", sizeof(struct _t_DestroyList));
        return NULL;
    }
    cli->tClient      = tClient;
    INIT_LIST_HEAD(&(cli->list));
    return cli;
}


void Add_DestroyObj_To_Tail(struct _t_DestroyList *cli)
{
    pthread_mutex_lock(&mutex_destroy);

    INIT_LIST_HEAD(&(cli->list));

	//printf("[%s][%s:%d] |  fd: %d | Terminal: %p | Terminal_Ptr: %p | recv_event=%d\n", __FILE__, __FUNCTION__, __LINE__, (*(cli->tClient))->fd, *(cli->tClient), cli->tClient, (*(cli->tClient))->recv_event);
					
    list_add_tail(&(cli->list), &head_destroy);
	
    pthread_cond_broadcast(&cond_destroy);
    pthread_mutex_unlock(&mutex_destroy);	
}

int Is_DestroyList_Empty()
{
    return list_empty(&head_destroy);
}

struct _t_DestroyList *Get_DestroyObj_From_Head()
{
    struct list_head      *cur = NULL;
    struct _t_DestroyList *ptr = NULL;
    struct list_head      *tmp = NULL;
    list_for_each_safe(cur, tmp, &head_destroy)
    {
        ptr = container_of(cur, struct _t_DestroyList, list);
        return ptr;
    }
    return NULL;
}

struct _t_DestroyList *Remove_DestroyObj_From_Head()
{
    struct _t_DestroyList *ptr = NULL;
    ptr = Get_DestroyObj_From_Head();
    if(ptr != NULL)
        list_del_init(&(ptr->list));
    return ptr;
}

void AddToDestroyManagerList(struct _t_Terminal **tClient)
{
	if (NULL == tClient)
		return;
	
	struct _t_DestroyList *cli = Create_DestroyObj(tClient);
    if (NULL != cli)
    {
        Add_DestroyObj_To_Tail(cli);
    }
	return;
}

void HandleDestroyThreadfunc(void *arg)
{
	printf("[%s][%s:%d] | Thread : %ld\n",  __FILE__, __FUNCTION__, __LINE__, syscall(SYS_gettid));

    while(!StopDestroy)
    {
    	usleep(DESTROY_INTERVAL_MILLISECOND);
        pthread_mutex_lock(&mutex_destroy);
        while ( Is_DestroyList_Empty() )
        {
            pthread_cond_wait(&cond_destroy, &mutex_destroy);
        }
        struct _t_DestroyList *tCli = Remove_DestroyObj_From_Head();
		struct _t_Terminal **tClient = tCli->tClient;
		tCli->tClient = NULL;
		free(tCli);
        pthread_mutex_unlock(&mutex_destroy);

        Destory_TcpTerminalObj(tClient);
    }

    StopDestroy = 0;
}


void InitDestroyManager()
{
    int ret;
    pthread_mutex_init(&mutex_destroy, NULL);
    pthread_cond_init(&cond_destroy, NULL);

    pthread_t destroy_thread_tid[DESTROY_THREADPOOL_SUM];

    int i;
    for (i = 0; i < DESTROY_THREADPOOL_SUM; ++i)
    {
        ret = pthread_create(&(destroy_thread_tid[i]) , NULL, (void *)&HandleDestroyThreadfunc, NULL);
        if (-1 == ret)
        {
            return;
        }
        ret = pthread_detach(destroy_thread_tid[i]);
        if (-1 == ret)
        {
            return;
        }
    }

    printf("[%s][%s:%d]\n", __FILE__, __FUNCTION__, __LINE__);
}

void StopDestroyManager()
{
    StopDestroy = 1;
    struct _t_DestroyList  *ptr = NULL;
    while(1)
    {
        ptr = Remove_DestroyObj_From_Head();
        if(ptr == NULL)
        {
            break;
        }
		free(ptr);
        ptr = NULL;
    }
}

