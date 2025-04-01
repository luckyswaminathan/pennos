######################################################################
#
#                       Author: Hannah Pan
#                       Date:   01/13/2021
#
# The autograder will run the following command to build the program:
#
#       make -B
#
# To build a version that does not call kill(2), it will run:
#
#       make -B CPPFLAGS=-DEC_NOKILL
#
######################################################################

# name of the program to build
#
PROG=penn-shell
PROMPT='"$(PROG)> "'

# Remove -DNDEBUG during development if assert(3) is used
#
override CPPFLAGS += -DNDEBUG -DPROMPT=$(PROMPT)

CC = clang-15

# Replace -O1 with -g for a debug version during development
#
# NOTE aadalal: added -Wno-gnu-statement-expression to support macro based generic containers
# (like linked_list.h, vector.h) that use statement expressions.
CFLAGS += -g3 -gdwarf-4 -Wall -Werror -Wpedantic -Wno-gnu-statement-expression -Wno-language-extension-token -I. -I.. -I../src --std=gnu2x
CXXFLAGS += -g3 -gdwarf-4 -Wall -Werror -Wpedantic -I. -I.. --std=gnu++2b

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard *.h)

# Testing configuration
TEST_PROG = test_runner
TEST_DIR = tests
TEST_HEADERS = $(wildcard $(TEST_DIR)/*.hpp)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS = $(TEST_SRCS: .cpp=.o)

YOUR_SRCS = $(filter-out parser.c, $(SRCS))
YOUR_HEADERS = $(filter-out parser.h, $(HEADERS))

.PHONY : all clean tidy-check format test

all: $(PROG) tidy-check

$(PROG) : $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) ../src/scheduler.o ../src/spthread.o

%.o: %.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

clean :
	$(RM) $(OBJS) $(PROG)

tidy-check: 
	clang-tidy-15 \
        --extra-arg=--std=gnu2x \
        -warnings-as-errors=* \
        -header-filter=.* \
        $(YOUR_SRCS) $(YOUR_HEADERS)

$(TEST_DIR)/%.o: $(TEST_SRCS) $(TEST_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

$(TEST_PROG): $(OBJS) $(TEST_OBJS)
	$(CC) $(CPPFLAGS) -o $@ $(TEST_OBJS) $(OBJS)

test: $(TEST_PROG)
	./$(TEST_PROG)

format:
	clang-format-15 -i --verbose --style=Chromium $(YOUR_SRCS) $(YOUR_HEADERS)

