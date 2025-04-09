SRC_DIR = src
BIN_DIR = bin
LOG_DIR = log
DOC_DIR = doc
TESTS_DIR = tests

.PHONY: all tests info format clean

CC = clang-15
CXX = clang++-15
CFLAGS = -g3 -gdwarf-4 -pthread -Wall -Werror -Wno-gnu -O0 -g --std=gnu2x
CXXFLAGS = -g3 -gdwarf-4 -pthread -Wall -Werror -Wno-gnu -O0 -g --std=gnu++2b

# tells it to search for 
CPPFLAGS = -I $(SRC_DIR)

# add each test name to this list
# for example:
# TEST_MAINS = $(TESTS_DIR)/test1.c $(TESTS_DIR)/othertest.c $(TESTS_DIR)/sched-demo.c
TEST_MAINS = $(TESTS_DIR)/sched-demo.c 

# list all files with their own main() function here
# for example:
# MAIN_FILES = $(SRC_DIR)/stand_alone_pennfat.c $(SRC_DIR)/helloworld.c $(SRC_DIR)/pennos.c
MAIN_FILES = $(SRC_DIR)/pennos.c 

# to get the executables, remove the .c from the filename and put 
# it in the BIN_DIR
EXECS = $(subst $(SRC_DIR),$(BIN_DIR),$(MAIN_FILES:.c=))
TEST_EXECS = $(subst $(TESTS_DIR),$(BIN_DIR),$(TEST_MAINS:.c=))

# srcs = all C files in SRC_DIR that are not listed in MAIN_FILES
SRCS = src/scheduler.c src/spthread.c src/logger.c shell/exiting_alloc.c src/kernel.c src/sys.c
HDRS = src/scheduler.h src/spthread.h src/logger.h shell/exiting_alloc.h src/kernel.h lib/linked_list.h src/sys.h
MAIN = src/sched-test.c

TEST_OBJS = $($(wildcard $(TESTS_DIR)/*.c):.c=.o)

all: $(EXECS)

tests: $(TEST_EXECS)

$(EXECS): $(BIN_DIR)/%: $(SRC_DIR)/%.c $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRCS) $<

$(TEST_EXECS): $(BIN_DIR)/%: $(TESTS_DIR)/%.c $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRCS) $(subst $(BIN_DIR)/,$(TESTS_DIR)/,$@).c

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ -c $<

info:
	$(info MAIN_FILES: $(MAIN_FILES)) \
	$(info EXECS: $(EXECS)) \
	$(info SRCS: $(SRCS)) \
	$(info HDRS: $(HDRS)) \
	$(info OBJS: $(OBJS)) \
	$(info TEST_MAINS: $(TEST_MAINS)) \
	$(info TEST_EXECS: $(TEST_EXECS))

format:
	clang-format -i --verbose --style=Chromium $(MAIN_FILES) $(TEST_MAINS) $(SRCS) $(HDRS)

clean:
	rm $(OBJS) $(EXECS) $(TEST_EXECS)
