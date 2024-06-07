#  override CFLAGS := -std=gnu99 -O0 -Wall -Werror -g -fsanitize=undefined $(CFLAGS) -I.
#  override LDFLAGS := -fsanitize=undefined -fsanitize=leak $(LDFLAGS)  
CC = gcc
 
# I generally make the first rule run all the tests
all: check

# rule for making the parser.o  that is needed by all the test programs
myshell_parser.o: myshell_parser.c myshell_parser.h
myshell.o: myshell.c myshell_parser.h

myshell : myshell.o myshell_parser.o

# Add any additional tests here
test_files= ./myshell

.PHONY: clean check checkprogs all

# Build all of the test program
checkprogs: $(test_files)

check: checkprogs
	$(test_files)

clean:
	rm -f *~ *.o $(test_files) $(test_o_files)
