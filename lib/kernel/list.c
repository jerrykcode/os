#include "list.h"
#include "debug.h"
#include "stddef.h"
#include "stdbool.h"

void list_init(struct list_st *list) {
    ASSERT(list != NULL);
    list->head = NULL;
    list->tail = NULL;
}

void list_push_back(struct list_st *list, list_node node) {
    ASSERT(list != NULL);
    ASSERT(node != NULL);

    if (list->head == NULL) {
        ASSERT(list->tail == NULL);
        list->head = list->tail = node;
        node->pre = node->next = NULL;        
    }
    else {
        ASSERT(list->head && list->tail);
        list->tail->next = node;
        node->pre = list->tail;
        node->next = NULL;
        list->tail = node;        
    }
}

void list_push_front(struct list_st *list, list_node node) {
    ASSERT(list != NULL);
    ASSERT(node != NULL);

    if (list->head == NULL) {
        ASSERT(list->tail == NULL);
        list->head = list->tail = node;
        node->pre = node->next = NULL;
    }
    else {
        ASSERT(list->head && list->tail);
        node->next = list->head;
        node ->pre = NULL;
        list->head->pre = node;
        list->head = node;
    }
}

list_node list_pop(struct list_st *list) {
    ASSERT(list != NULL);

    if (list->head == NULL) {
        ASSERT(list->tail == NULL);
        return NULL;
    }
    ASSERT(list->head && list->tail);
    list_node head = list->head;
    if (head->next) {
        head->next->pre = NULL;
        list->head = head->next;
    }
    else {
        list->head = list->tail = NULL;
    }
    return head;
}

bool list_empty(struct list_st *list) {
    ASSERT(list != NULL);

    if (list->head == NULL) {
        ASSERT(list->tail == NULL);
        return true;
    }
    ASSERT(list->head && list->tail);
    return false;
}

bool list_exist(struct list_st *list, list_node node) {
    ASSERT(list != NULL);

    list_node ptr = list->head;
    while (ptr) {
        if (ptr == node)
            return true;
        ptr = ptr->next;
    }
    return false;
}

// 调用此函数需要确保list中存在node
// 此函数中不进行判断
void list_remove(struct list_st *list, list_node node) {
    if (node == NULL)
        return;
    list_node pre = node->pre;
    list_node next = node->next;
    if (pre)
        pre->next = next; // 跨过node
    else // pre不存在，说明node是head, 移除node则head需要更新为next
        list->head = next;

    if (next)
        next->pre = pre; // 跨过node
    else // next不存在，说明node是tail, 移除node则需要更新tail为pre
        list->tail = pre;
}

void list_traversal(struct list_st *list, bool (*func)(list_node, int), int arg) {
    list_node node = list->head;
    while (node) {
        if (func(node, arg))
            break;
        node = node->next;
    }
}
