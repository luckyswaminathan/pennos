#include <assert.h>

// Implementation of a doubly linked list.
// It's heavily inspired by the vector.h
// header from hw1.
//
// Usage:
// typedef struct int_ll_node_st {
//   int_ll_node_st* prev; // MUST be named prev
//   int_ll_node_st* next; // MUST be named next
//   int val;
// } int_ll_node;
// 
// linked_list(int_ll_node) int_ll = linked_list_new(int_ll_node, NULL);
// ...

typedef void (*destroy_fn)(void*);

/* Handle for the linked list which has the head and tail.
 */
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define linked_list(T) struct { T * head; T * tail; destroy_fn ele_dtor; }

#define linked_list_head(self) \
  ({                           \
    (self)->head;                 \
  })


#define linked_list_tail(self) \
  ({                           \
    (self)->tail;                 \
  })

#define linked_list_push_head(self, ele, prev_field, next_field) \
  ({                                     \
   if ((self)->head == NULL) {              \
      (self)->head = ele;                   \
      (self)->tail = ele;                   \
      (ele)->prev_field = NULL;                  \
      (ele)->next_field = NULL;                  \
    } else {                             \
      (self)->head->prev_field = ele;             \
      (ele)->next_field = (self)->head;             \
      (ele)->prev_field = NULL;                  \
      (self)->head = ele;                   \
    }                                    \
  })


#define linked_list_push_tail(self, ele, prev_field, next_field) \
  ({                                     \
   if ((self)->head == NULL) {              \
      (self)->head = ele;                   \
      (self)->tail = ele;                   \
      (ele)->prev_field = NULL;                  \
      (ele)->next_field = NULL;                  \
    } else {                             \
      (self)->tail->next_field = ele;             \
      (ele)->prev_field = (self)->tail;             \
      (ele)->next_field = NULL;                  \
      (self)->tail = ele;                   \
    }                                    \
  })

/**
 * Note: if ele does not belong to the list
 * then this is indeterminate
 */
#define linked_list_remove(self, ele, prev_field, next_field) \
  ({                             \
    if ((ele) == NULL) { \
      /* Do nothing */ \
    } else { \
      if ((ele)->next_field != NULL) { \
        (ele)->next_field->prev_field = (ele)->prev_field; \
        assert((self)->tail != (ele)); \
      } else { \
        assert((self)->tail == (ele)); \
        (self)->tail = (ele)->prev_field; \
      }\
      if ((ele)->prev_field != NULL) { \
        (ele)->prev_field->next_field = (ele)->next_field; \
        assert((self)->head != (ele)); \
      } else { \
        assert((self)->head == (ele)); \
        (self)->head = (ele)->next_field; \
     } \
     (ele)->prev_field = NULL; \
     (ele)->next_field = NULL; \
   } \
  })

#define linked_list_pop_head(self, prev_field, next_field)  \
  ({                                     \
    typeof((self)->head) __ret = (self)->head;  \
    linked_list_remove(self, (self)->head, prev_field, next_field); \
    __ret;                               \
  })


#define linked_list_pop_tail(self, prev_field, next_field)       \
  ({                                     \
    typeof((self)->tail) __ret = (self)->tail;             \
    linked_list_remove(self, (self)->tail, prev_field, next_field); \
    __ret;                               \
  })

#define linked_list_next(ele, prev_field, next_field) \
  ({                          \
    (ele)->next_field;                \
  })

#define linked_list_prev(ele, prev_field, next_field) \
  ({                          \
    (ele)->prev_field;                \
  })

#define linked_list_clear(self, prev_field, next_field)          \
  ({                                     \
    typeof((self)->head) __ele = (self)->head; \
    typeof((self)->head) __next_ele;        \
    (self)->head = NULL;                    \
    (self)->tail = NULL;                    \
    while (__ele != NULL) {              \
      __next_ele = __ele->next_field;          \
      __ele->prev_field = NULL;                \
      __ele->next_field = NULL;                \
      if ((self)->ele_dtor != NULL) {       \
        (self)->ele_dtor(__ele);            \
      }                                  \
      __ele = __next_ele;                \
    }                                    \
  })

