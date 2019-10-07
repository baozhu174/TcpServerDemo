/*
 * kernel_list.h
 * 描述：
 *        此为内核链表结构(循环链表)
 */
     
#pragma once
      
#include <stdio.h>
#include <stddef.h>

struct list_head
{
    struct list_head *prev;
    struct list_head *next;
};
             
#define LIST_HEAD(name) struct list_head name={&(name),&(name)}
              
    /*
     * 初始化链表
     */
    static inline void INIT_LIST_HEAD(struct list_head *node)
    {
        node->prev = node;
        node->next = node;
    }
                         
/*
 * 在prev与next节点之间插入节点new_node
 */
static inline void __list_add(struct list_head *new_node,struct list_head *prev,struct list_head *next)
{
    prev->next = new_node;
    next->prev = new_node;
    new_node->prev = prev;
    new_node->next = next;
}
                                         
/*
 * 头插
 */
static inline void list_add(struct list_head *new_node,struct list_head *head)
{
    __list_add(new_node,head,head->next);
}
                                             
/*
 * 尾插
 */
static inline void list_add_tail(struct list_head *new_node,struct list_head *head)
{
    __list_add(new_node,head->prev,head);
}
                                                 
/*
 * 删除节点
 */
static inline void list_del(struct list_head *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}
                                                         
/*
 * 删除节点并初始化删除的节点
 */
static inline void list_del_init(struct list_head *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    INIT_LIST_HEAD(node);
}
                                                                     
/*
 * 判断链表是否为空
 */
static inline int list_empty(struct list_head *node)
{
    return node->next == node;
}

#ifndef offsetof
/*
 * offsetof见stddef.h中:
 * TYPE是某struct的类型 0是一个假想TYPE类型struct,MEMBER是该struct中的一个成员.
 * 由于该struct的基地址为0, MEMBER的地址就是该成员相对与struct头地址的偏移量.
 */
#define offsetof(type, member)                  \
    ( (size_t)(&((type*)0)->member) )
#endif

/*
 * typeof,这是gcc的C语言扩展保留字,用于声明变量类型.
 * typeof 关键字将用做类型名（typedef名称）并指定类型
 *       typeof(int *) p1, p2;  －> int *p1, *p2;
 *      typeof(int [10]) a1, a2; --> int a1[10], a2[10];
 *     typeof(int) * p3, p4; -> int *p3,p4;
 *
 */
/**
 * container_of - cast a member of a structure out to the containing structure
 * container_of在Linux Kernel中的应用非常广泛,它用于获得某结构中某成员的入口地址.container_of见kernel.h中:
 * @ptr:     the pointer to the member.ptr是成员变量的指针
 * @type:     the type of the container struct this is embedded in.type是指结构体的类型
 * @member:     the name of the member within the struct.member是成员变量的名字
 *const typeof( ((type *)0->member ) *__mptr = (ptr);
 *     意思是声明一个与member同一个类型的指针常量 *__mptr,并初始化为ptr.
 *， ， 。
 *(type *)( (char *)__mptr - offsetof(type,member) );
 *     意思是__mptr的地址减去member在该struct中的偏移量得到的地址, 再转换成type型指针. 该指针就是member的入口地址了.
 */
#define container_of(ptr, type, member)                     \
    ({ typeof(((type*)0)->member) *__mptr = ptr;            \
        (type*)((char*)__mptr - offsetof(type, member)); })
                                                                                                  
/*
 * 从头节点后面的节点开始遍历链表
 */
#define list_for_each(cur, head)                \
    for (cur = (head)->next; (cur) != (head);   \
         cur = (cur)->next)
                                                                                                                 
/*
 * 遍历链表,并对cur的节点进行操作
 */
#define list_for_each_safe(cur, tmp, head)              \
    for (cur = (head)->next, tmp = (cur)->next;         \
         (cur) != (head); cur = tmp, tmp = (tmp)->next)
/*
 * 反向遍历
 */
#define list_for_each_reverse(cur, head)        \
    for (cur = (head)->prev; (cur) != (head);   \
         cur = (cur)->prev)
/*
 * 从cur节点后的节点开始遍历链表
 */
#define list_for_each_continue(cur, head)       \
    for (cur = (cur)->next; (cur) != (head);    \
         cur = (cur)->next)
                                                                                                                                                     
#define list_for_each_from(cur, head)           \
    for (; (cur) != (head); cur = (cur)->next)
                                                                                                                                                         
/*
 * 从头节点的下一个节点开始遍历，一直末尾
 */
#define list_for_each_entry(ptr, head, member)                          \
    for ( ptr = container_of((head)->next, typeof(*(ptr)), member);     \
          &(ptr)->member != (head);                                     \
          ptr = container_of((ptr)->member.next, typeof(*(ptr)), member) )
                                                                                                                                                                             
/*
 * 从头节点的下一个节点开始遍历，一直末尾,可以进行删除操作
 */
#define list_for_each_entry_safe(ptr, tmp, head, member)                \
    for ( ptr = container_of((head)->next, typeof(*(ptr)), member),     \
              tmp = container_of((ptr)->member.next, typeof(*(ptr)), member); \
          &(ptr)->member != (head);                                     \
          ptr = tmp,                                                    \
              tmp = container_of((tmp)->member.next, typeof(*(ptr)), member) )
