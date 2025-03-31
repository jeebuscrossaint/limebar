#!/bin/bash

BG="#1a002b"
NET_COLOR="#00ff00"
LOAD_COLOR="#ff606c"
MEM_COLOR="#ffa753"
TIME_COLOR="#97bcd3"

get_network_speed() {
    local iface="wlan0"  # Change this to your interface
    local rx1=$(cat /sys/class/net/$iface/statistics/rx_bytes)
    local tx1=$(cat /sys/class/net/$iface/statistics/tx_bytes)
    sleep 1
    local rx2=$(cat /sys/class/net/$iface/statistics/rx_bytes)
    local tx2=$(cat /sys/class/net/$iface/statistics/tx_bytes)

    local rxKB=$(( (rx2 - rx1) / 1024 ))
    local txKB=$(( (tx2 - tx1) / 1024 ))
    echo "$rxKB $txKB"
}

while true; do
    read RX TX <<< $(get_network_speed)
    LOAD=$(uptime | awk -F'average: ' '{print $2}' | cut -d, -f1)
    MEM=$(free | grep Mem | awk '{print int($3/$2 * 100)}')
    TIME=$(date "+%H:%M:%S")

    OUTPUT=""
    OUTPUT+="[F=$NET_COLOR:↓${RX}KB/s ↑${TX}KB/s] "
    OUTPUT+="[F=$LOAD_COLOR: ${LOAD}] "
    OUTPUT+="[F=$MEM_COLOR: ${MEM}%] "
    OUTPUT+="[F=$TIME_COLOR: ${TIME}]"

    echo "$OUTPUT"
done | ./limebar -g 1920x24+0+0 -B "$BG" -f "FiraCode Nerd Font 11" -u 2
