#!/bin/bash

echo "==========================="
echo "    Starting e2e tests     "
echo "==========================="
echo

displayID=:1

# Launch Xephyr
Xephyr -ac -screen 800x600 $displayID &
xephyrPID=$!
# echo "Xephyr PID: $xephyrPID"
sleep 0.5

DISPLAY=$displayID

# Launch WM
esy run &
wmPID=$!
# echo "WM PID: $wmPID"

sleep 0.5

# Launch Kitty
kitty &
kittyPID=$!
# echo "Kitty PID: $wmPID"

sleep 2

# Kill everything
kill $kittyPID
kill $wmPID
sleep 0.5
kill $xephyrPID

echo
echo "==========================="
echo "   e2e tests complete :)   "
echo "==========================="
echo
