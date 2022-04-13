from track import Track
from font import font_med
from virtual_screen import VirtualScreen
from scene import TextOnlyScene
from rpi_ws281x import Color
from screen import Screen
import datetime
import sys
import time

def main(screen):
    change = True
    screen_height = screen.height()
    screen_width =  screen.width()
    scene = TextOnlyScene(screen=screen, inter_track_space=1)
    scene.generate_tracks()
    scene.tracks[0].do_not_scroll = True
    second = None
    prev_second = None

    print(scene.tracks)
 
    while True:
        current = datetime.datetime.now()
        current_hr = current.strftime("%H")
        current_min = current.strftime("%M")

        if change:
            current_hr = current_hr + ":"
        change = not change
        scene.clear_tracks(0)
        scene.add_text_to_track(current_hr, 0)        

        if current_min != scene.track_value(1):
            scene.clear_tracks(1)
            scene.add_text_to_track(current_min, 1, (233, 75, 60))        

        scene.frame()

        time.sleep(1)

if __name__ == '__main__':
    if "-h" in sys.argv or "--help" in sys.argv:
        show_usage()
        exit(0)
    test_virtual_screen = False
    screen_height = 16
    screen_width = 16
    if "--virtual" in sys.argv:
        index = sys.argv.index("--virtual")
        if len(sys.argv) < 4 or len(sys.argv[index::]) < 3:
            show_usage()
            exit(1)
        test_virtual_screen = True
        screen_height = int(sys.argv[index + 1])
        screen_width = int(sys.argv[index + 2])
        screen = VirtualScreen(screen_height, screen_width)
    else:
        screen = Screen()
    main(screen)


