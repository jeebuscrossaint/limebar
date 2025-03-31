#!/usr/bin/env bash

# Colors
CPU_COLOR="#ff606c"   # Red
HIGH_LOAD="#ff0000"   # Bright red for high usage
MED_LOAD="#ffff00"    # Yellow for medium usage
LOW_LOAD="#00ff00"    # Green for low usage
ICON_COLOR="#f66a22"  # Orange for icon

get_cpu_usage() {
    # Get CPU usage percentage
    top -bn1 | grep "Cpu(s)" | sed "s/.*, *\([0-9.]*\)%* id.*/\1/" | awk '{print 100 - $1}'
}

# Check if raw mode is requested
if [ "$1" == "raw" ]; then
    # Raw mode
    while true; do
        CPU=$(get_cpu_usage)
        printf "CPU: %.1f%%\n" $CPU
        sleep 1
    done | ./limebar -r -F "#ff606c" -B "#1a1a1a" -p 10
else
    # Formatted block mode
    while true; do
        CPU=$(get_cpu_usage)

        # Set color based on usage
        if [ $(echo "$CPU > 80" | bc -l) -eq 1 ]; then
            COLOR=$HIGH_LOAD
        elif [ $(echo "$CPU > 50" | bc -l) -eq 1 ]; then
            COLOR=$MED_LOAD
        else
            COLOR=$LOW_LOAD
        fi

        # Format with blocks
        # Using  as CPU icon (needs Nerd Font)
        printf "[F=$ICON_COLOR:  ] [F=$COLOR:CPU %.1f%%]\n" $CPU

        sleep 1
    done | ./limebar -B "#1a1a1a" -f "FiraCode Nerd Font 11" -p 10
fi
