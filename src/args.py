import argparse

def parseargs():
        parser = argparse.ArgumentParser("limebar")
        parser.add_argument("-h", help="Show this help message and exit")
        parser.add_argument("-g widthxheight+x+y", help="Set the window geometry. If a parameter is omitted it's filled with the default value. If the y parameter is specified along with the -b switch then the position is relative to the bottom of the screen.")
        parser.add_argument("-o name", help="Set next output to name. May be used multiple times; order is significant. If any -o options are given, only -o specified monitors will be used. Invalid output names are silently ignored. (only supported on randr configurations at this time)")
        parser.add_argument("-b", help="Dock the bar at the bottom of the screen.")
        parser.add_argument("-d", help="Force docking without asking the window manager. This is needed if the window manager isn't EWMH compliant.")
        parser.add_argument("-f font", help="Specify the font to use. Can be used multiple times to load more than a single font.")
        parser.add_argument("-p", help="Make the bar permanent, don't exit after the standard input is clsoed.")
        parser.add_argument("-n name", help="Set the WM_NAME atom value for the bar")
        parser.add_argument("-u pixel", help="Sets the underline width in pixels. The defualt is 1.")
        parser.add_argument("-B color", help="Set the background color of the bar. color must be specified in the hex format (#aarrggbb, #rrggbb, #rgb). If no compositor such as compton or xcompmgr is running the alpha channel is silently ignored.")
        parser.add_argument("-F color", help="Set the foreground color of the bar. Accepts the same color formats as -B.")
        parser.add_argumnet("-U color", help="Set the underline color of the bar. Accepts the same color formats as -B.")

        return parser.parse_args()

