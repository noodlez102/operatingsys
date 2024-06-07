# EC440: Shell Parser

The sample code provided for the Parser includes:
- [Makefile](Makefile):  The Makefile that builds your parser, tests, and runs your test programs as described [here](https://openosorg.github.io/openos/textbook/intro/tools-make.html#a-simple-example)
- [myshell_parser.h](myshell_parser.h): The header file that describes the structures and interfaces that clients of your parser needs to know (see [this](https://openosorg.github.io/openos/textbook/intro/tools-testing.html#testing)).
- [myshell_parser.c](myshell_parser.c): An initial stub implementation of the functions that you will need to fill in.
- [run_tests.sh](run_tests.sh): A shell script, described [here](https://openosorg.github.io/openos/textbook/intro/tools-shell.html#shell) for automating running all the tests

Two example test programs that exercise the interface of the parser, discussed [here](https://openosorg.github.io/openos/textbook/intro/tools-testing.html#testing), are:
- [test_simple_input.c](test_simple_input.c): Tests parsing the command 'ls'
- [test_simple_pipe.c](test_simple_pipe.c): Tests parsing the command 'ls | cat`

Please modify this README.md file to describe any interesting strategy you used to develop the parser and list any references to resources you used to develop your solution. Also, please add a line above for each test program you develop. 

Currently developing the shell based on the hints given to us and the 