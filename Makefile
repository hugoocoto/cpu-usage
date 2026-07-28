all: test

test: test.c cpu_usage.h
	gcc -Wall -Wextra test.c -o test
