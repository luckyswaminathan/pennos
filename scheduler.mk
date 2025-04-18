CC = clang
CFLAGS = -g3 -gdwarf-4 -Wall -Werror -Wpedantic -Wno-gnu-statement-expression -Wno-language-extension-token --std=gnu2x
CPPFLAGS = -DNDEBUG -I. -I./src -I./src/scheduler -I./lib

# Scheduler files
SCHED_SRCS = src/scheduler/scheduler.c src/scheduler/spthread.c src/scheduler/logger.c lib/exiting_alloc.c 
SCHED_HDRS = src/scheduler/scheduler.h src/scheduler/spthread.h src/scheduler/logger.h lib/exiting_alloc.h lib/linked_list.h
SCHED_OBJS = $(SCHED_SRCS:.c=.o)
SCHED_TEST = scheduler-test

.PHONY: all clean scheduler

all: scheduler

scheduler: $(SCHED_TEST)

$(SCHED_TEST): $(SCHED_OBJS) $(SCHED_HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SCHED_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(SCHED_TEST) $(SCHED_OBJS) 