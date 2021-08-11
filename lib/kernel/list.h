#ifndef LIB_LIST_H__
#define LIB_LIST_H__
#include "stdint.h"
#include "stdbool.h"

typedef struct list_node_st {
    struct list_node_st *pre;
    struct list_node_st *next;
} *list_node;

struct list_st {
    list_node head;
    list_node tail;
};

void list_init(struct list_st *list);
void list_push_back(struct list_st *list, list_node node); // 插入到队列尾
void list_push_front(struct list_st *list, list_node node); // 插入到队列头
list_node list_pop(struct list_st *list); // 从队列头弹出
bool list_empty(struct list_st *list);
bool list_exist(struct list_st *list, list_node node);
void list_remove(struct list_st *list, list_node node);
void list_traversal(struct list_st *list, bool (*func)(list_node, int), int arg);

#endif
