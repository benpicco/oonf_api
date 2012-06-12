#!/bin/sh

# ../src-api/regex/* is OpenBSD
EXCEPT="$EXCEPT -not -path '../src-api/regex/*'"

# ../src-api/android/linux/* is Linux Kernel
EXCEPT="$EXCEPT -not -path '../src-api/android/linux/*'"

LEN=`cat default_licence.txt |wc -c`

for file in $(eval find ../src*  -type f -name *[.][ch] $EXCEPT)
do
  cmp --bytes $LEN $file default_licence.txt
  if [ $? != 0 ]
  then
    # do nothing
    true
  fi
done
