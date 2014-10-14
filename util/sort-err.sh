#!/bin/csh -f
sed -e 's/^\(.*\) incorrect [(]\(.*\)$/\2 \1/' | sort -n
