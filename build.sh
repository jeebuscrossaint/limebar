#!/bin/sh
set -e

cc -g -Wall -Wextra \
  $(pkg-config --cflags wayland-client cairo) \
  -I. \
  -o limebar \
  main.c \
  wlr-layer-shell-unstable-v1-client-protocol.c \
  xdg-shell-client-protocol.c \
  $(pkg-config --libs wayland-client cairo) \
  -lwayland-client
