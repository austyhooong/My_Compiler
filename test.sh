#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() {return 3;}
int ret5() {return 5;}
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }
int add6(int a, int b, int c, int d, int e, int f) {
  return a+b+c+d+e+f;
}
EOF

assert() {
  expected="$1"
  input="$2"

  ./au_cc "$input" > tmp.s || exit
  gcc -static -o tmp tmp.s tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}


assert 3 '{ return ret3(); }'
assert 5 '{ return ret5(); }'
assert 8 '{return add(3, 5); }'
assert 2 '{return sub(5, 3); }'
assert 21 '{return add6(1, 10, 3, 4, 2, 1); }'
assert 10 '{return add(5, add(2, 3));}'
assert 136 '{ return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }'

echo Good Job!

