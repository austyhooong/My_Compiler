#!/bin/bash
# create a temp direcdtory with random name filled in X
tmp=`mktemp -d /tmp/chibicc-test-XXXXXX`

# create a default command to run when INT TEMP HUP EXIT signals are generated
trap 'rm -rf $tmp' INT TERM HUP EXIT
echo > $tmp/empty.c

check() {
    # check the exit status
    if [ $? -eq 0 ]; then
        echo "testing $1 ... passed"
    else
        echo "testing $1 ... failed"
        exit 1
    fi
}

# -o: when output file is specified
rm -f $tmp/out
./au_cc -o $tmp/out $tmp/empty.c
[ -f $tmp/out ]
check -o

# --help; -q: silently output the result to the exit status
./au_cc --help 2>&1 | grep -q au_cc
check --help

echo GOOD JOB!