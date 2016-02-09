#!/bin/sh
BTADDR=`hciconfig | sed -n '2p' | awk -F' ' '{print $3}'`
if [ "$BTADDR" != "" ]; then
	echo $BTADDR > /var/local/last_btaddress
fi

/usr/local/bin/sixpair `cat /var/local/last_btaddress` > /tmp/sixpair

while [ 1 ]; do
	sleep 2;
	if [ -c /dev/input/js0 ]; then 
		/usr/local/bin/mw-ps3
	fi

done

