# limebar

## NAME

limebar - Featherweight lime-scented bar

## SYNOPSIS

```
❯ ./result/bin/limebar -h
Usage: ./result/bin/limebar [options]

Options:
  -r, --raw              Enable raw text mode (no block parsing)
  -F, --text-color COLOR Set default text color for raw mode
  -g, --geometry WxH+X+Y    Set bar geometry (e.g., 1920x24+0+0)
  -B, --background COLOR    Set background color (e.g., #1a1a1a)
  -f, --font FONT          Add font (can be used multiple times)
  -u, --underline SIZE     Set underline thickness (default: 2)
  -p, --padding SIZE       Set text padding (default: 10)
  -a, --alignment POS      Set default alignment (left|center|right)
  -t, --position POS       Set bar position (top|bottom)
  -m, --margin MARGINS     Set margins (top,right,bottom,left)
  -s, --separator STRING   Set block separator
  -o, --opacity FLOAT      Set background opacity (0.0-1.0)
  -h, --help              Show this help message
```

## DESCRIPTION

**limebar** (only known as limebar) is a lightweight statusbar based on wayland.
Provides stdin reading, and block type formatting.

## INPUT

The data to be parsed is read from the standard input, parsing and printing the
input data are delayed until a newline is found.

### WWW

[git repository](https://github.com/jeebuscrossaint/limebar)

### AUTHOR

Amarnath P. (jeebuscrossaint) 2025 - AD INFINITUM

Hefty credit for inspiration from [LemonBoy](https://github.com/LemonBoy/bar)
