#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() {return 3;}
int ret5() {return 5;}
EOF