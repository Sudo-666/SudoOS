#pragma once
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief 获取包含某成员的结构体指针
 * @param ptr 指向成员的指针
 * @param type 结构体类型
 * @param member 成员名称
 * @return type* 指向包含该成员的结构体的指针
 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// 通用双向循环链表节点
typedef struct list_node {
    struct list_node *next;
    struct list_node *prev;
} list_node_t;

/**
 * @brief 初始化节点（指向自己）
 * 
 * @param node 
 */
static inline void list_init(list_node_t *node) {
    node->next = node;
    node->prev = node;
}

/**
 * @brief 在两个节点之间插入新节点
 * 
 * @param new_node 
 * @param prev 
 * @param next 
 */
static inline void __list_add(list_node_t *new_node, list_node_t *prev, list_node_t *next) {
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

/**
 * @brief 插入到某个节点之后
 *  
 * @param new_node list_node_t *
 * @param head list_node_t *
 */
static inline void list_add_after(list_node_t *new_node, list_node_t *head) {
    __list_add(new_node, head, head->next);
}

/**
 * @brief 插入到某个节点之前（如果是头节点，则相当于插入到尾部）
 * 
 * @param new_node 
 * @param head 
 */
static inline void list_add_before(list_node_t *new_node, list_node_t *head) {
    __list_add(new_node, head->prev, head);
}

/**
 * @brief 删除节点
 * 
 * @param node 
 */
static inline void list_del(list_node_t *node) {
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->next = node; // 防止悬空
    node->prev = node;
}