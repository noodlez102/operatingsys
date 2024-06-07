# Set compiler

CC = gcc



# Compiler flags

#CFLAGS := -Wall -Werror -std=gnu99 -O0 -g $(CFLAGS) -I.



# Uncomment to add sanitizer for development

#CFLAGS :=  -fsanitize=undefined $(CFLAGS) 

#LDFLAGS := -fsanitize=undefined -fsanitize=leak $(LDFLAGS)  



# Test files

test_files=./test_busy_threads ./barriertest ./mutextest



# Build only threads.o by default

all: threads.o



# Build threads.o

threads.o: threads.c ec440threads.h

	$(CC) $(CFLAGS) -c threads.c -o threads.o



# Rules to build each of the tests

test_busy_threads.o: test_busy_threads.c threads.c

test_busy_threads: test_busy_threads.o threads.o

	$(CC) $(CFLAGS) test_busy_threads.o threads.o -o test_busy_threads



mutextest.o : mutextest.c threads.c 

mutextest: mutextest.o threads.o

	$(CC) $(CFLAGS) mutextest.o threads.o -o mutextest



barriertest.o : barriertest.c threads.c 

barriertest: barriertest.o threads.o

	$(CC) $(CFLAGS) barriertest.o threads.o -o barriertest



.PHONY: clean check 



# Build and run the test programs

check: threads.o test_busy_threads mutextest barriertest

	/bin/bash run_tests.sh $(test_files)



clean:

	rm -f *.o $(test_files) $(test_o_files)