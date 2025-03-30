sched-demo:
	clang -o bin/sched-demo test/sched-demo.c src/utils/spthread.c -I. -pthread --std=gnu2x

standalone-pennfat: $(wildcard src/pennfat/*.c) $(wildcard src/pennfat/*.h)
	clang -o bin/standalone-pennfat src/pennfat/*.c -I.
