#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./au_cc "$input" > tmp.s || exit
  gcc -static -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 0
assert 42 42
assert 47 "5+6*7"
assert 4 "(3+5) / 2"
assert 10 "-10+20"
assert 10 "- -10"
echo OK

