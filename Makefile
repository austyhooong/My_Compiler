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

#phony distinguishes target from file (file is prioritized)

#wildcard: help expands ouside of rule within variable or inside arguments of function
#-within the rule, if no matched files found, the original pattern remains (ex: *.c); but in the above case, results in blank!
# %: matches nonempty string

CFLAGS=-std=c11 -g -fno-common
CC=gcc

#expand by space separated result
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TEST_SRCS=$(wildcard test/*.c)
TESTS=$(TEST_SRCS:.c=.exe)

au_cc: $(OBJS)
	$(CC) $(CFLAG) -o $@ $^ $(LDFLAGS)

$(OBJS): au_cc.h

# gcc source_file object_file -o output_file
# -o- output the binary result to stdout
# -E: output preprocessed code
# -P: omit the line directives (contains line number and used for debugging)
# -C: retain the comments in the preprocessed code
# -xc: specifies the source file extension as c file
# pass test c soure files as input to ./au_cc to convert it to assembly
# ASSERT is defined in test/common which compares the second argument which has been parsed by the compiler with given input

test/%.exe: au_cc test/%.c
	$(CC) -o- -E -P -C test/$*.c | ./au_cc -o test/$*.s -
	$(CC) -o $@ test/$*.s -xc test/common

# $: makefile variable
# $$: shell variable
test: $(TESTS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/test-driver.sh

# --rm : close container after the command
docker: clean
	docker run --rm -v /Users/ahong107/Desktop/Austin_s_compiler/austins_compiler:/austin_compiler -w /austin_compiler compilerbook_x86_64 make test


# '(': specifies the start of group of condition \
-o : OR operator \
'*~': matches file name that ends with ~ (temporary by convension)
# -exec rm: execute rm on the matched prior condition \
{}: placeholder for the matched result \
';': needed to terminate -exec
clean:
	rm -rf au_cc tmp* $(TESTS) test/*.s test/*.exe
	find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'

.PHONY: test clean