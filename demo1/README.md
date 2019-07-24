# 样例代码
文件 dequeue.c 和 enqueue.c 取自 Userspace RCU 项目目录，网址：
- https://github.com/urcu/userspace-rcu/tree/master/doc/examples/rculfqueue

# 头文件
头文件`rculfqueue.h`提供RCU无锁队列的抽象数据类型：
```
#include <urcu/rculfqueue.h>	/* RCU Lock-free queue */
```

# 函数名前缀规则
1. 无锁队列rculfqueue.h库: 所有函数名和结构体名前缀均为`cds_lfq_`，其中lfq是lock-free queue的缩写；
   - cds_lfq_node_init_rcu(node)
   - cds_lfq_init_rcu(queue)
   - cds_lfq_destroy_rcu()

2. 顶层urcu.h库提供了两个前缀为`rcu_`的库函数: `rcu_register_thread()`和`rcu_unregister_thread()`;

# API 关键数据结构体名称
`struct cds_lfq_node_rcu` 和 `struct cds_lfq_queue_rcu`

# 伪代码展示无锁队列API函数调用步骤
## 节点定义
```
struct mynode {
	int value;			/* 定义节点内容格式，此处以int整数为例 */
	struct cds_lfq_node_rcu qnode;	/* Chaining in queue */
	struct rcu_head my_rcu_head;	/* For call_rcu() */
};
```

## 多线程并发程序逻辑

【父线程：负责初始化myqueue, 启动N个生产者线程, 然后启动N个消费者线程】
```
struct cds_lfq_queue_rcu myqueue;
cds_lfq_init_rcu(&myqueue, call_rcu); // 初始化myqueue, 设置并发模式

pthread_create(..., producer_func, (void *)&myqueue);
pthread_create(..., consumer_func, (void *)&myqueue);
```

【子线程1作为生产者(producer), 每间隔几秒向缓存队列 myqueue 中注入一条新数据】
```
const int N_VALUES=3;
int demo_values[N_VALUES] = {1, 2, 3};
struct mynode *ptr = NULL;
while (1) {
	int i;
	for (i=0; i<N_VALUES; i++) {
		ptr = malloc(sizeof(struct mynode));
		ptr->value = demo_values[i];
		cds_lfq_node_init_rcu(&(ptr->qnode));
		rcu_read_lock();
		cds_lfq_enqueue_rcu(myqueue_ptr, &(ptr->qnode))
		rcu_read_unlock();
		ptr = NULL;

		// 随机睡眠 0.5 - 3.0秒
		float time = generate_random_number_between(0.5, 3.0);
		sleep_sevaral_seconds(time);
	}
}
```

【子线程2：作为消费者(consumer)，从缓存队列 myqueue 取出数据, 处理之后销毁数据】
```
struct cds_lfq_node_rcu *qnode_ptr;
struct mynode           *ptr;
while (1) {
	rcu_read_lock();
	qnode_ptr = cds_lfq_dequeue_rcu(myqueue_ptr);
	rcu_read_unlock();
	if (!qnode_ptr) {
		sleep_sevaral_seconds(0.5);// 此时myqueue队列暂时没有新节点可供读取. 间隔 0.5 秒后重试一次
		continue;
	}

	ptr = caa_container_of(qnode_ptr, struct mynode, qnode); // 通过qnode_ptr反向推导出外层结构体mynode的大小

	// 输出数据
	printf(" %d", ptr->value);

	// 释放int value对应的内存资源
	call_rcu(&node->my_rcu_head, free_node);

	sleep_sevaral_seconds(1.0);
}

//...

static
void free_node(struct rcu_head *head)
{
	struct mynode *ptr = NULL;

	ptr = caa_container_of(head, struct mynode, my_rcu_head);
	free(ptr);
}
```

# 附录1: 头文件 compiler.h 节选
1. 宏定义函数 caa_container_of(指向成员变量的指针, mynode结构体, 成员变量名)
```
/*
 * caa_container_of - Get the address of an object containing a field.
 *
 * @ptr: pointer to the field.
 * @type: type of the object.
 * @member: name of the field within the object.
 */
#define caa_container_of(ptr, type, member)				\
	__extension__							\
	({								\
		const __typeof__(((type *) NULL)->member) * __ptr = (ptr); \
		(type *)((char *)__ptr - offsetof(type, member));	\
	})
```
# 附录2: 头文件 rculfqueue.h 节选

```
struct cds_lfq_node_rcu {
	struct cds_lfq_node_rcu *next;
	int dummy;
};

struct cds_lfq_queue_rcu {
	struct cds_lfq_node_rcu *head, *tail;

};

extern void cds_lfq_node_init_rcu(struct cds_lfq_node_rcu *node);
extern void cds_lfq_init_rcu(struct cds_lfq_queue_rcu *q,
			     void queue_call_rcu(struct rcu_head *head,
					void (*func)(struct rcu_head *head)));
/*
 * The queue should be emptied before calling destroy.
 *
 * Return 0 on success, -EPERM if queue is not empty.
 */
extern int cds_lfq_destroy_rcu(struct cds_lfq_queue_rcu *q);

/*
 * Should be called under rcu read lock critical section.
 */
extern void cds_lfq_enqueue_rcu(struct cds_lfq_queue_rcu *q,
				struct cds_lfq_node_rcu *node);

/*
 * Should be called under rcu read lock critical section.
 *
 * The caller must wait for a grace period to pass before freeing the returned
 * node or modifying the cds_lfq_node_rcu structure.
 * Returns NULL if queue is empty.
 */
extern
struct cds_lfq_node_rcu *cds_lfq_dequeue_rcu(struct cds_lfq_queue_rcu *q);

//...
```
