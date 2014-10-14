#!/bin/csh -f
set count="$1"
shift
head -8c /dev/zero
echo "$*"
head -8c /dev/zero
head -${count}c /dev/urandom
exit
