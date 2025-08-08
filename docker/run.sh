#!/bin/bash

# Start virtual display for GUI apps
Xvfb :99 -screen 0 1024x768x16 &

export DISPLAY=:99

echo "Running in $(pwd), contents:"
ls -l

# Launch the application binary — adjust binary name if needed
./uvgComm "$@"