CC = clang
CFLAGS = -g3 -gdwarf-4 -Wall -Werror -Wpedantic -Wno-gnu-statement-expression -Wno-language-extension-token --std=gnu2x
CPPFLAGS = -DNDEBUG -I. -I./shell -I./scheduler

# Scheduler files
SCHED_SRCS = scheduler/scheduler.c scheduler/spthread.c scheduler/logger.c ../lib/exiting_alloc.c scheduler/kernel.c scheduler/sys.c
SCHED_HDRS = scheduler/scheduler.h scheduler/spthread.h scheduler/logger.h ../lib/exiting_alloc.h scheduler/kernel.h ../lib/linked_list.h scheduler/sys.h
SCHED_OBJS = $(SCHED_SRCS:.c=.o)

# Shell files
SHELL_PROG = shell/penn-shell
SHELL_SRCS = $(wildcard shell/*.c)
SHELL_HDRS = $(wildcard shell/*.h)
SHELL_OBJS = $(SHELL_SRCS:.c=.o)

.PHONY: all clean scheduler shell

all: scheduler shell

scheduler: $(SCHED_TEST)

shell: $(SHELL_PROG)

$(SCHED_TEST): scheduler/sched-test.c $(SCHED_OBJS) $(SCHED_HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SCHED_OBJS) scheduler/sched-test.c

$(SHELL_PROG): $(SHELL_OBJS) $(SCHED_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SHELL_OBJS) $(SCHED_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(SCHED_TEST) $(SHELL_PROG) $(SCHED_OBJS) $(SHELL_OBJS)

tidy-check:
	clang-tidy-15 \
	--extra-arg=--std=gnu2x \
	-warnings-as-errors=* \
	-header-filter=.* \
	$(SHELL_SRCS) $(SHELL_HDRS)

format:
	clang-format-15 -i --verbose --style=Chromium $(SHELL_SRCS) $(SHELL_HDRS)
