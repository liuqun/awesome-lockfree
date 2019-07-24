/*
 * Copyright (C) 2013  Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program for any
 * purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is
 * granted, provided the above notices are retained, and a notice that
 * the code was modified is included with the above copyright notice.
 *
 * This example shows how to dequeue nodes from a RCU lock-free queue.
 * This queue requires using a RCU scheme.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <urcu.h>		/* RCU flavor */
#include <urcu/rculfqueue.h>	/* RCU Lock-free queue */
#include <urcu/compiler.h>	/* For CAA_ARRAY_SIZE */

/*
 * 用户数据容器
 */
struct container {
	/* 此示例程序中无锁队列中的用户数据是 int value */
	int value;
	/* 结构体成员变量 member2 是一个lfq无锁队列结点结构体 */
	struct cds_lfq_node_rcu member2;
	/* 结构体成员变量 member3 仅用于辅助执行RCU回调函数 */
	struct rcu_head rcu_head;
};

extern void *get_container_ptr_by_member2(struct cds_lfq_node_rcu *member2);
extern void *get_container_ptr_by_member3(struct rcu_head *member3);

extern struct container *CONTAINER_new(int init_value);
extern void              CONTAINER_free(struct container *ptr);

typedef struct container *CONTAINER;


int main(int argc, char **argv)
{
	int values[] = { -5, 42, 36, 24, };
	unsigned int TOTAL_VALUES = sizeof(values)/sizeof(values[0]);
	struct cds_lfq_queue_rcu myqueue;   /* Queue */
	unsigned int i;
	int ret = 0;

	/*
	 * Each thread need using RCU read-side need to be explicitly
	 * registered.
	 */
	rcu_register_thread();

	cds_lfq_init_rcu(&myqueue, call_rcu);

	/*
	 * Enqueue nodes.
	 */
	for (i = 0; i < TOTAL_VALUES; i++) {
		struct container *container_ptr;

		container_ptr = CONTAINER_new(values[i]);
		if (!container_ptr) {
			ret = -1;
			goto end;
		}

		/*
		 * Both enqueue and dequeue need to be called within RCU
		 * read-side critical section.
		 */
		rcu_read_lock();
		cds_lfq_enqueue_rcu(&myqueue, &container_ptr->member2);
		rcu_read_unlock();
	}

	/*
	 * Dequeue each node from the queue. Those will be dequeued from
	 * the oldest (first enqueued) to the newest (last enqueued).
	 */
	printf("dequeued content:");
	for (;;) {
		struct cds_lfq_node_rcu *member2_ptr;
		struct container *container_ptr;

		/*
		 * Both enqueue and dequeue need to be called within RCU
		 * read-side critical section.
		 */
		rcu_read_lock();
		member2_ptr = cds_lfq_dequeue_rcu(&myqueue);
		rcu_read_unlock();
		if (!member2_ptr) {
			break;  /* Queue is empty. */
		}
		/* Getting the container structure from its member2 (from the "member2" inside "struct container") */
		container_ptr = get_container_ptr_by_member2(member2_ptr);
		printf(" %d", container_ptr->value);
		CONTAINER_free(container_ptr);
	}
	printf("\n");
	/*
	 * Release memory used by the queue.
	 */
	ret = cds_lfq_destroy_rcu(&myqueue);
	if (ret) {
		printf("Error destroying queue (non-empty)\n");
	}
end:
	rcu_unregister_thread();
	return ret;
}

/* 自定义函数 */

/*
 * 计算指针偏移量, 推导出外层 container 结构体的首字节地址
 */
void *get_container_ptr_by_member2(struct cds_lfq_node_rcu *member2)
{
	uint8_t *container_ptr = (uint8_t *) member2;

	container_ptr -= offsetof(struct container, member2);
	return container_ptr;
}

/*
 * 根据计算指针偏移量, 推导出外层 container 结构体的首字节地址
 */
void *get_container_ptr_by_member3(struct rcu_head *member3)
{
	uint8_t *container_ptr = (uint8_t *) member3;

	container_ptr -= offsetof(struct container, rcu_head);
	return container_ptr;
}

/*
 * CONTAINER 类的构造函数
 */
struct container *CONTAINER_new(int init_value)
{
	struct container *ptr = NULL;
	ptr = malloc(sizeof(struct container));
	if (!ptr) {
		#ifndef WRITE_ERROR_LOG
		#define WRITE_ERROR_LOG(format, ...) fprintf(stderr, format, __VA_ARGS__)
		#endif
		WRITE_ERROR_LOG("Error: System is out of memory: %s\n", strerror(errno));
		WRITE_ERROR_LOG("[SOURCE=%s:LINE=%d:errno=%d]\n", __FILE__, __LINE__, errno);
		abort();
	}
	ptr->value = init_value;
	cds_lfq_node_init_rcu(&(ptr->member2));
	memset(&ptr->rcu_head, 0x00, sizeof(ptr->rcu_head));
	return (ptr);
}

static
void CONTAINER_free_callback(struct rcu_head *rcu_head_ptr)
{
	struct container *container_ptr = NULL;

	/* Getting the container structure from its member3 (from the "rcu_head" inside "struct container") */
	container_ptr = get_container_ptr_by_member3(rcu_head_ptr);
	free(container_ptr);
}

/*
 * CONTAINER 类的析构函数
 */
void CONTAINER_free(struct container *ptr)
{
	call_rcu(&ptr->rcu_head, CONTAINER_free_callback);
}
