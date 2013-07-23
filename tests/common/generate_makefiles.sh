#!/bin/bash

for file in *.c; do
	test=$(basename $file .c)
	sed s/%TESTNAME%/$test/ .Makefile.template > Makefile.$test
done
