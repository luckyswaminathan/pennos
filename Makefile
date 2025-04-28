SRC_DIR = src
TESTS_DIR = tests
BIN_DIR = bin


CC = clang
CFLAGS = -g3 -gdwarf-4 -Wall -Werror -Wpedantic -Wno-gnu-statement-expression -Wno-language-extension-token --std=gnu2x
CPPFLAGS = -DNDEBUG -I. -I.. -I./shell -I./scheduler

# Scheduler files
SCHED_SRCS = src/scheduler/scheduler.c src/scheduler/spthread.c src/scheduler/logger.c lib/exiting_alloc.c src/scheduler/kernel.c src/scheduler/fat_syscalls.c src/pennfat/fat.c src/pennfat/fat_utils.c src/scheduler/sys.c src/utils/errno.c
SCHED_HDRS = src/scheduler/scheduler.h src/scheduler/spthread.h src/scheduler/logger.h lib/exiting_alloc.h src/scheduler/kernel.h lib/linked_list.h src/scheduler/sys.h src/scheduler/fat_syscalls.h src/pennfat/fat.h src/pennfat/fat_utils.h src/pennfat/fat_constants.h src/utils/errno.h src/utils/error_codes.h
SCHED_OBJS = $(SCHED_SRCS:.c=.o)

# Shell files
SHELL_PROG = bin/pennos
SHELL_SRCS = $(wildcard src/shell/*.c)
SHELL_HDRS = $(wildcard src/shell/*.h)
SHELL_OBJS = $(SHELL_SRCS:.c=.o)

.PHONY: all clean scheduler shell dirs pennfat-info pennfat-all

all: dirs scheduler shell

scheduler: $(SCHED_TEST)

shell: $(SHELL_PROG)

$(SCHED_TEST): src/scheduler/sched-test.c $(SCHED_OBJS) $(SCHED_HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SCHED_OBJS) src/scheduler/sched-test.c

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

# configs for standalone pennfat
# and pennfat tests
PENNFAT_MAIN = $(SRC_DIR)/pennfat/pennfat.c
PENNFAT_SRCS = $(filter-out $(PENNFAT_MAIN), $(wildcard $(SRC_DIR)/pennfat/*.c))
PENNFAT_HDRS = $(wildcard $(SRC_DIR)/pennfat/*.h)
PENNFAT_EXEC = $(BIN_DIR)/pennfat
PENNFAT_OBJS = $(PENNFAT_SRCS:.c=.o)
PENNFAT_TEST_HDRS = $(TESTS_DIR)/pennfat/acutest.h
PENNFAT_TEST_MAIN = $(TESTS_DIR)/pennfat/test_pennfat.c
PENNFAT_TEST_EXEC = $(TESTS_DIR)/pennfat/test_pennfat

pennfat-info:
	$(info PENNFAT_MAIN: $(PENNFAT_MAIN)) \
	$(info PENNFAT_SRCS: $(PENNFAT_SRCS)) \
	$(info PENNFAT_SRCS: $(PENNFAT_SRCS)) \
	$(info PENNFAT_HDRS: $(PENNFAT_HDRS)) \
	$(info PENNFAT_EXEC: $(PENNFAT_EXEC)) \
	$(info PENNFAT_OBJS: $(PENNFAT_OBJS)) \
	$(info PENNFAT_TEST_HDRS: $(PENNFAT_TEST_HDRS)) \
	$(info PENNFAT_TEST_MAIN: $(PENNFAT_TEST_MAIN)) \
	$(info PENNFAT_TEST_EXEC: $(PENNFAT_TEST_EXEC))

pennfat-all: $(PENNFAT_EXEC) $(PENNFAT_TEST_EXEC)

$(PENNFAT_TEST_EXEC): $(PENNFAT_OBJS) $(PENNFAT_TEST_MAIN) 

$(PENNFAT_EXEC): $(PENNFAT_OBJS) $(PENNFAT_MAIN)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@

$(PENNFAT_OBJS): %.o: %.c $(PENNFAT_HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<
