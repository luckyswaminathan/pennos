#define CATCH_CONFIG_MAIN
#include "catch.hpp"

extern "C" {
  #include "../linked_list.h"
}

using namespace std;

/* Int linked list  
 * 
 * Note: this is the example in the header file.
 **/

typedef struct int_ll_node_st {
    int_ll_node_st* prev;
    int_ll_node_st* next;
    int val;
} int_ll_node;

TEST_CASE("int linked_list_new", "[linked_list]") {
    linked_list(int_ll_node) int_ll = linked_list_new(int_ll_node, NULL);
    // REQUIRE(linked_list_head(int_ll) == NULL);
    // REQUIRE(linked_list_tail(int_ll) == NULL);
}
