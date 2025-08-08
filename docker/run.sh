#!/bin/bash

# Start virtual display for GUI apps, use this if not using X-forwarding
# Xvfb :99 -screen 0 1024x768x16 &
#export DISPLAY=:99

export DISPLAY=${DISPLAY:-:0}

echo "Display: " 
echo $DISPLAY

echo "Running in $(pwd), contents:"
ls -l


# List available cameras
v4l2-ctl --list-devices || true

# list video files
echo "Videos: "
ls -l /dev/video*

chmod a+rw /dev/video*

# Launch the application binary — adjust binary name if needed
./uvgComm "$@"