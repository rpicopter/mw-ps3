#!/bin/sh
while [ 1 ]; do
sleep 2;
if [ -c /dev/input/js0 ]; then 
	/usr/local/bin/mw-ps3
fi

done
