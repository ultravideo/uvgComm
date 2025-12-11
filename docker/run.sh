#!/bin/bash

# Default: try to use virtual screen
USE_VIRTUAL_SCREEN=true

# if DISPLAY is defined, lets use it
if [ ! -z "$DISPLAY" ]; then
    USE_VIRTUAL_SCREEN=false
    echo "Using host display"
else
    USE_VIRTUAL_SCREEN=true
fi

if [ "$USE_VIRTUAL_SCREEN" = true ]; then
    # Start Xvfb on :99
    export DISPLAY=:99
    Xvfb $DISPLAY -screen 0 1280x720x24 &
    XVFB_PID=$!
    echo "Virtual screen started with PID $XVFB_PID"
fi

echo "Display: $DISPLAY"
if [ -z "$KV_HEADLESS_FORCE_OFFSCREEN" ]; then
    echo "KV_HEADLESS_FORCE_OFFSCREEN: <unset>"
else
    echo "KV_HEADLESS_FORCE_OFFSCREEN=$KV_HEADLESS_FORCE_OFFSCREEN"
fi
echo "Contents of /uvgcomm:"
ls -l /uvgcomm

echo "Running in $(pwd), contents:"
ls -l

echo "Video devices:"
shopt -s nullglob
video_found=false
for dev in /dev/video*; do
    chmod a+rw "$dev"
    echo "$dev"
    video_found=true
done
shopt -u nullglob
[ "$video_found" = false ] && echo "No video devices found"

# Launch uvgComm
./uvgComm "$@"
echo "uvgComm exited with code $? at $(date)"

if [ "$USE_VIRTUAL_SCREEN" = true ]; then
    kill $XVFB_PID
fi
