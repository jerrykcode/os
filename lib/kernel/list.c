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
