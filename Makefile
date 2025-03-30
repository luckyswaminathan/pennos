sched-demo:
	clang -o bin/sched-demo test/sched-demo.c src/utils/spthread.c -I. -pthread --std=gnu2x
