CC = clang
CFLAGS = -g3 -pthread -Wall -Werror -Wno-gnu -O0 --std=gnu2x
CPPFLAGS = -I.

# Source files
SRCS = scheduler.c spthread.c logger.c
HDRS = scheduler.h spthread.h logger.h
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
