# -*- MakeFile -*-

# target: dependencies
#	action
# -target is usually the name of a file (executable/object files) or an action
# if target is more outdated than dependencies, make starts off with dependenciese; if they have the rule, these are executed first 

# library path is needed for the linker to link the libary into an executable; both of below are needed for linking
# -L(dir) : specifies the directory in which library resides
# -l(library name)

# \
$@: the target filename. \
$*: the target filename without the file extension.\
$<: the first prerequisite filename.\
$^: the filenames of all the prerequisites, separated by spaces, discard duplicates. \
$+: similar to $^, but includes duplicates. \
$?: the names of all prerequisites that are newer than the target, separated by spaces.

# .c file is implicitly converted to .o file; no need make a rule
CFLAGS=-std=c11 -g -fno-common
CC=gcc
#expand by space separated result
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
au_cc: $(OBJS)
	$(CC) $(CFLAG) -o $@ $^ $(LDFLAGS)

$(OBJS): au_cc.h

test: clean au_cc
	./test.sh

# --rm : close container after the command
docker:
	docker run --rm -v /Users/ahong107/Desktop/Austin_s_compiler/austins_compiler:/austin_compiler -w /austin_compiler compilerbook_x86_64 make test

clean:
	rm -f au_cc *.o *~ tmp* *.out main

.PHONY: test clean