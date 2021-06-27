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
void list_push(struct list_st *list, list_node node);
list_node list_pop(struct list_st *list);
bool list_empty(struct list_st *list);
bool list_exist(struct list_st *list, list_node node);

#endif
