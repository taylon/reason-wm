#!/bin/bash

echo "Running tests..."

displayID=:1

Xephyr -ac -screen 800x600 $displayID &
xephyrPID=$!

DISPLAY=$displayID

esy run &
wmPID=$!

kitty &
kittyPID=$!

sleep 3

# Kill all
kill $wmPID
kill $kittyPID
sleep 1
kill $xephyrPID
