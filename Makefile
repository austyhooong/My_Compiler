# -*- MakeFile -*-

# target: dependencies
#	action
# if target is more outdated than dependencies, make starts off with dependenciese; if they have the rule, these are executed first 

# library path is needed for the linker to link the libary into an executable; both of below are needed for linking
# -L(dir) : specifies the directory in which library resides
# -l(library name)

#$@: the target filename. \
$*: the target filename without the file extension.\
$<: the first prerequisite filename.\
$^: the filenames of all the prerequisites, separated by spaces, discard duplicates. \
$+: similar to $^, but includes duplicates. \
$?: the names of all prerequisites that are newer than the target, separated by spaces.

CFLAGS=-std=c11 -g -fno-common
CC=gcc

au_cc: main.o
	$(CC) -o au_cc main.o $(LDFLAGS)

test: clean au_cc
	./test.sh

docker:
	docker run --rm -v ${MY}:/austin_compiler -w /austin_compiler compilerbook_x86_64 make test

clean:
	rm -f au_cc *.o *~ tmp* *.out main

.PHONY: test clean