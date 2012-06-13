#!/bin/sh

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
