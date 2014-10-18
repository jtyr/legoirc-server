#!/bin/bash

# PID file location
PIDFILE=/var/run/legoirc-server.pid

# Return value
RETVAL=0


function start() {
    if [ -e $PIDFILE ]; then
        echo "PID file exists"
        RETVAL=1
        return
    fi

    # Start the server on background
    /usr/bin/legoirc-server $OPTIONS 1>/var/log/legoirc-server.log 2>&1 &

    echo $! > $PIDFILE
}


function stop() {
    # Kill the main process and all its children
    kill -- $(ps -o pid= -s $(ps -o sess --no-heading --pid $(cat $PIDFILE)) | grep -o '[0-9]*')

    rm -f $PIDFILE
}


PARAM=$1
echo "Action: $PARAM"

case $PARAM in
    'start')
        start
        shift
        ;;
    'stop')
        stop
        shift
        ;;
    *)
        echo "ERROR: Unknown action: $PARAM"
        shift
        ;;
esac

exit $RETVAL
