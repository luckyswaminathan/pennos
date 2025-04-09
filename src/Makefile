CC = clang
CFLAGS = -g3 -pthread -Wall -Werror -Wno-gnu -O0 --std=gnu2x
CPPFLAGS = -I. -I../shell

# Source files
SRCS = scheduler.c spthread.c logger.c ../shell/exiting_alloc.c kernel.c sys.c
HDRS = scheduler.h spthread.h logger.h ../shell/exiting_alloc.h kernel.h ../lib/linked_list.h sys.h
MAIN = sched-test.c
OBJS = $(SRCS:.c=.o)

# Output executable
TARGET = sched-test

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(MAIN) $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(OBJS) $(MAIN)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS) *.o
