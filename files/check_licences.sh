#!/bin/sh

LEN=`cat ./files/default_licence.txt |wc -c`

OKAY=0
BAD=0

EXCEPT="-not -wholename ./src-plugins/nl80211_listener/nl80211.h"
EXCEPT="${EXCEPT} -not -wholename ./src-plugins/eth_listener/ethtool-copy.h"
for file in $(eval find ./src* ./tests ./examples -type f -name *[.][ch] ${EXCEPT})
do
	cmp --bytes ${LEN} ${file} ./files/default_licence.txt
	if [ ${?} != 0 ]
	then
		BAD=$((${BAD} + 1))
	else
		OKAY=$((${OKAY} + 1))
	fi
done

TOTAL=$((${OKAY} + ${BAD}))

if [ ${OKAY} != 0 ]
then
	echo "Found ${OKAY} source/header files with the correct header"
fi
if [ ${BAD} != 0 ]
then
	echo "Found ${BAD} source/header files with the wrong or an outdated header"
fi
if [ ${TOTAL} = 0 ]
then
	echo "No files found, please run script from the main directory of the repository"
fi
