CC = clang
CFLAGS = -g3 -gdwarf-4 -Wall -Werror -Wpedantic -Wno-gnu-statement-expression -Wno-language-extension-token --std=gnu2x
CPPFLAGS = -DNDEBUG -I. -I./src -I./src/scheduler -I./lib

# Scheduler files
SCHED_SRCS = src/scheduler/scheduler.c src/scheduler/spthread.c src/scheduler/logger.c lib/exiting_alloc.c
SCHED_HDRS = src/scheduler/scheduler.h src/scheduler/spthread.h src/scheduler/logger.h lib/exiting_alloc.h lib/linked_list.h
SCHED_OBJS = $(SCHED_SRCS:.c=.o)
SCHED_TEST = scheduler-test

# Test files
TEST_SRCS = tests/test_scheduler.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_BIN = test_scheduler

.PHONY: all clean scheduler test

all: scheduler test

scheduler: $(SCHED_TEST)

test: $(TEST_BIN)

$(SCHED_TEST): $(SCHED_OBJS) $(TEST_OBJS) $(SCHED_HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SCHED_OBJS) $(TEST_OBJS)

$(TEST_BIN): $(TEST_OBJS) $(SCHED_OBJS) $(SCHED_HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_OBJS) $(SCHED_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(SCHED_TEST) $(SCHED_OBJS) $(TEST_BIN) $(TEST_OBJS) 