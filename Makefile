sched-demo:
	clang -o bin/sched-demo test/sched-demo.c src/utils/spthread.c -I. -pthread --std=gnu2x

pennfat: $(wildcard src/pennfat/*.c) $(wildcard src/pennfat/*.h)
	clang -o bin/pennfat src/pennfat/*.c -I. -g
