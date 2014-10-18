#!/bin/bash

# PID file location
PIDFILE=/var/run/vlc-camera-stream.pid

# Return value
RETVAL=0


function start() {
    if [ -e $PIDFILE ]; then
        echo "PID file exists"
        RETVAL=1
        return
    fi

    # Start the streaming on background
    /opt/vc/bin/raspivid -o - $CAM_RASPIVID_OPTIONS | /usr/bin/cvlc -vvv stream:///dev/stdin --sout "#standard{access=http,mux=ts,dst=:${CAM_PORT}}" :demux=h264 1>/dev/null 2>&1 &
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
