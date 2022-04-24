"""Usage:
  led_grid serve [--port=<port>] [--screen_type=(virtual|physical)] [--height <virtual_screen_height>] [--width <virtual_screen_width>]
  led_grid set (pixel|bg|img) (<pixel>|<bg>|<img>)
  led_grid open <context>
  led_grid show (virtual|physical)
"""
from docopt import docopt
import os
import led_grid

def control_hw(**kwargs):
    print("Control HW")

def main():
    arguments = docopt(__doc__)
    if arguments['serve']:
        port = 5000
        virtual_screen = True
        virtual_screen_height = 16
        virtual_screen_width = 16
        if arguments["--screen_type"] and arguments["--screen_type"] != "virtual":
            virtual_screen = False 
        if arguments['--port']:
            port = int(arguments['--port'])
        led_grid.run(port, virtual_screen, virtual_screen_height, virtual_screen_width)

if __name__ == "__main__":
    main()
