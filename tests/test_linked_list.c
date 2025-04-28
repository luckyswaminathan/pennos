/**
 * @file test_linked_list.c
 * @brief Unit tests for the linked_list.h implementation
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../linked_list.h"

/* Test node structure for integers */
typedef struct int_node_st {
    struct int_node_st* prev;  // MUST be named prev
    struct int_node_st* next;  // MUST be named next
    int val;
} int_node;

/* Test node structure for strings */
typedef struct str_node_st {
    struct str_node_st* prev;  // MUST be named prev
    struct str_node_st* next;  // MUST be named next
    char* str;
} str_node;

/* Function to free string nodes */
void free_str_node(void* node) {
    str_node* n = (str_node*)node;
    free(n->str);
    free(n);
}

/* Function to free integer nodes */
void free_int_node(void* node) {
    free(node);
}

/* Helper function to create an integer node */
int_node* create_int_node(int val) {
    int_node* node = malloc(sizeof(int_node));
    node->prev = NULL;
    node->next = NULL;
    node->val = val;
    return node;
}

/* Helper function to create a string node */
str_node* create_str_node(const char* str) {
    str_node* node = malloc(sizeof(str_node));
    node->prev = NULL;
    node->next = NULL;
    node->str = strdup(str);
    return node;
}

/* Test function to verify list operations */
void test_int_list() {
    printf("Testing integer linked list...\n");
    
    /* Create a new list */
    linked_list(int_node) int_list = linked_list_new(int_node, free_int_node);
    assert(linked_list_is_empty(&int_list));
    assert(linked_list_size(&int_list) == 0);
    
    /* Add elements to the list */
    int_node* node1 = create_int_node(1);
    int_node* node2 = create_int_node(2);
    int_node* node3 = create_int_node(3);
    
    linked_list_push_tail(&int_list, node1);
    assert(!linked_list_is_empty(&int_list));
    assert(linked_list_size(&int_list) == 1);
    assert(linked_list_head(&int_list) == node1);
    assert(linked_list_tail(&int_list) == node1);
    
    linked_list_push_tail(&int_list, node2);
    assert(linked_list_size(&int_list) == 2);
    assert(linked_list_head(&int_list) == node1);
    assert(linked_list_tail(&int_list) == node2);
    assert(linked_list_next(node1) == node2);
    assert(linked_list_prev(node2) == node1);
    
    linked_list_push_head(&int_list, node3);
    assert(linked_list_size(&int_list) == 3);
    assert(linked_list_head(&int_list) == node3);
    assert(linked_list_tail(&int_list) == node2);
    assert(linked_list_next(node3) == node1);
    assert(linked_list_prev(node1) == node3);
    
    /* Remove elements from the list */
    int_node* popped_head = linked_list_pop_head(&int_list);
    assert(popped_head == node3);
    assert(linked_list_size(&int_list) == 2);
    assert(linked_list_head(&int_list) == node1);
    
    int_node* popped_tail = linked_list_pop_tail(&int_list);
    assert(popped_tail == node2);
    assert(linked_list_size(&int_list) == 1);
    assert(linked_list_tail(&int_list) == node1);
    
    /* Clear the list */
    linked_list_clear(&int_list);
    assert(linked_list_is_empty(&int_list));
    assert(linked_list_size(&int_list) == 0);
    
    /* Free the popped nodes */
    free(popped_head);
    free(popped_tail);
    
    printf("Integer linked list tests passed!\n");
}

/* Test function to verify string list operations */
void test_str_list() {
    printf("Testing string linked list...\n");
    
    /* Create a new list */
    linked_list(str_node) str_list = linked_list_new(str_node, free_str_node);
    assert(linked_list_is_empty(&str_list));
    
    /* Add elements to the list */
    str_node* node1 = create_str_node("Hello");
    str_node* node2 = create_str_node("World");
    str_node* node3 = create_str_node("!");
    
    linked_list_push_tail(&str_list, node1);
    linked_list_push_tail(&str_list, node2);
    linked_list_push_head(&str_list, node3);
    
    assert(linked_list_size(&str_list) == 3);
    assert(strcmp(linked_list_head(&str_list)->str, "!") == 0);
    assert(strcmp(linked_list_tail(&str_list)->str, "World") == 0);
    
    /* Remove a node from the middle */
    str_node* middle = linked_list_next(linked_list_head(&str_list));
    linked_list_remove(&str_list, middle);
    assert(linked_list_size(&str_list) == 2);
    assert(strcmp(linked_list_head(&str_list)->str, "!") == 0);
    assert(strcmp(linked_list_tail(&str_list)->str, "World") == 0);
    assert(linked_list_next(linked_list_head(&str_list)) == linked_list_tail(&str_list));
    
    /* Free the removed node */
    free_str_node(middle);
    
    /* Clear the list */
    linked_list_clear(&str_list);
    assert(linked_list_is_empty(&str_list));
    
    printf("String linked list tests passed!\n");
}

/* Test function to verify edge cases */
void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    /* Create a new list */
    linked_list(int_node) list = linked_list_new(int_node, free_int_node);
    
    /* Test operations on empty list */
    assert(linked_list_pop_head(&list) == NULL);
    assert(linked_list_pop_tail(&list) == NULL);
    
    /* Test with a single element */
    int_node* node = create_int_node(42);
    linked_list_push_head(&list, node);
    assert(linked_list_size(&list) == 1);
    assert(linked_list_head(&list) == node);
    assert(linked_list_tail(&list) == node);
    
    /* Remove the only element */
    linked_list_remove(&list, node);
    assert(linked_list_is_empty(&list));
    free(node);
    
    printf("Edge case tests passed!\n");
}

int main() {
    printf("Running linked list tests...\n\n");
    
    test_int_list();
    printf("\n");
    
    test_str_list();
    printf("\n");
    
    test_edge_cases();
    printf("\n");
    
    printf("All tests passed!\n");
    return 0;
} 