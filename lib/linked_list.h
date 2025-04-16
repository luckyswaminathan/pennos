/**
 * @file linked_list.h
 * @brief A generic doubly linked list implementation using macros
 * 
 * This header provides a generic doubly linked list implementation using C macros.
 * It allows creating type-safe linked lists for any struct type.
 * 
 * Usage example:
 * @code
 * // Define a node structure
 * typedef struct int_node_st {
 *   struct int_node_st* prev;  // MUST be named prev
 *   struct int_node_st* next;  // MUST be named next
 *   int val;
 * } int_node;
 * 
 * // Create a new linked list
 * linked_list(int_node) int_list = {NULL, NULL, NULL};
 * 
 * // Add elements
 * int_node* node = malloc(sizeof(int_node));
 * node->val = 42;
 * linked_list_push_tail(&int_list, node, prev, next);
 * @endcode
 */

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <assert.h>
#include <stdlib.h>

/**
 * @brief Function pointer type for element destructor
 * 
 * This function is called when an element is removed from the list
 * and the list's ele_dtor field is set.
 */
typedef void (*destroy_fn)(void*);

/**
 * @brief Macro to define a linked list structure for type T
 * 
 * Note: Type T must have 'prev' and 'next' pointer fields of type T*
 */
#define linked_list(T) struct { \
    T* head;                    \
    T* tail;                    \
    destroy_fn ele_dtor;        \
}

/**
 * @brief Initialize a new linked list with a destructor function
 */
#define linked_list_new(T, dtor) {NULL, NULL, (destroy_fn)(dtor)}

/**
 * @brief Get the head of the linked list
 */
#define linked_list_head(self) ((self)->head)

/**
 * @brief Get the tail of the linked list
 */
#define linked_list_tail(self) ((self)->tail)

/**
 * @brief Check if the linked list is empty
 */
#define linked_list_is_empty(self) ((self)->head == NULL)

/**
 * @brief Push an element to the head of the list
 * 
 * @note The element type must have 'prev' and 'next' pointer fields
 */
#define linked_list_push_head(self, ele) \
  do { \
    if ((self)->head == NULL) { \
      (self)->head = (ele); \
      (self)->tail = (ele); \
      (ele)->prev = NULL; \
      (ele)->next = NULL; \
    } else { \
      (self)->head->prev = (ele); \
      (ele)->next = (self)->head; \
      (ele)->prev = NULL; \
      (self)->head = (ele); \
    } \
  } while (0)

/**
 * @brief Push an element to the tail of the list
 * 
 * @note The element type must have 'prev' and 'next' pointer fields
 */
#define linked_list_push_tail(self, ele) \
  do { \
    if ((self)->head == NULL) { \
      (self)->head = (ele); \
      (self)->tail = (ele); \
      (ele)->prev = NULL; \
      (ele)->next = NULL; \
    } else { \
      (self)->tail->next = (ele); \
      (ele)->prev = (self)->tail; \
      (ele)->next = NULL; \
      (self)->tail = (ele); \
    } \
  } while (0)

/**
 * @brief Remove an element from the list
 * 
 * @note The element type must have 'prev' and 'next' pointer fields
 */
#define linked_list_remove(self, ele) \
  do { \
    if ((ele) != NULL) { \
      if ((ele)->next != NULL) { \
        (ele)->next->prev = (ele)->prev; \
        assert((self)->tail != (ele)); \
      } else { \
        assert((self)->tail == (ele)); \
        (self)->tail = (ele)->prev; \
      } \
      if ((ele)->prev != NULL) { \
        (ele)->prev->next = (ele)->next; \
        assert((self)->head != (ele)); \
      } else { \
        assert((self)->head == (ele)); \
        (self)->head = (ele)->next; \
      } \
      (ele)->prev = NULL; \
      (ele)->next = NULL; \
    } \
  } while (0)

/**
 * @brief Pop the head element from the list
 * 
 * @note The element type must have 'prev' and 'next' pointer fields
 */
#define linked_list_pop_head(self) \
  ({ \
    typeof((self)->head) __ret = (self)->head; \
    if (__ret != NULL) { \
      linked_list_remove(self, __ret); \
    } \
    __ret; \
  })

/**
 * @brief Pop the tail element from the list
 * 
 * @note The element type must have 'prev' and 'next' pointer fields
 */
#define linked_list_pop_tail(self) \
  ({ \
    typeof((self)->tail) __ret = (self)->tail; \
    if (__ret != NULL) { \
      linked_list_remove(self, __ret); \
    } \
    __ret; \
  })

/**
 * @brief Get the next element in the list
 * 
 * @note The element type must have a 'next' pointer field
 */
#define linked_list_next(ele) ((ele)->next)

/**
 * @brief Get the previous element in the list
 * 
 * @note The element type must have a 'prev' pointer field
 */
#define linked_list_prev(ele) ((ele)->prev)

/**
 * @brief Clear all elements from the list
 * 
 * @note The element type must have 'prev' and 'next' pointer fields
 */
#define linked_list_clear(self) \
  do { \
    typeof((self)->head) __ele = (self)->head; \
    typeof((self)->head) __next_ele; \
    (self)->head = NULL; \
    (self)->tail = NULL; \
    while (__ele != NULL) { \
      __next_ele = __ele->next; \
      __ele->prev = NULL; \
      __ele->next = NULL; \
      if ((self)->ele_dtor != NULL) { \
        (self)->ele_dtor(__ele); \
      } \
      __ele = __next_ele; \
    } \
  } while (0)

/**
 * @brief Get the number of elements in the list
 * 
 * @note The element type must have a 'next' pointer field
 */
#define linked_list_size(self) \
  ({ \
    size_t __count = 0; \
    typeof((self)->head) __ele = (self)->head; \
    while (__ele != NULL) { \
      __count++; \
      __ele = __ele->next; \
    } \
    __count; \
  })

#endif /* LINKED_LIST_H */

