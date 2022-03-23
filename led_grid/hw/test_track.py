from track import Track
from screen import Screen
from font import font_med
from virtual_screen import VirtualScreen
import time
import sys

def test_content(height, width, excess):
    color = (255, 255, 255)
    content = []
    for i in range(width + excess):
        for j in range(height):
            content.append((255, (i * 10) % 256, (j * 10) % 256))
    print("Content:", height, width)
    return content

def calculate_tracks(screen, content_height, spacing):
    screen_height = screen.height()
    screen_width = screen.width()
    assert(content_height < screen_height)
    tracks = []
    track_count = int(screen_height / (content_height + spacing))
    horizontal_shift = spacing
    for i in range(track_count):
        tracks.append(Track(screen, content_height, screen_width, 0, horizontal_shift))
        horizontal_shift += spacing + content_height
    return tracks

def test_track(virtual_screen=False, v_height=16, v_width=16):
    if not virtual_screen:
        screen = Screen()
    else:
        screen = VirtualScreen(v_height, v_width)
    screen_height = screen.height()
    screen_width =  screen.width()
    font_height = font_med["height"]
    spacing = 1                     
    out_of_screen_cols = 12

    tracks = calculate_tracks(screen, font_height, spacing)
    for track in tracks:
        track.add_content(test_content(font_height, screen_width, out_of_screen_cols))
    
    print(track.get_contents_width())
   
    while(True): 
        for i, track in enumerate(tracks):
            print(f"Track {i}: Current Shift: {track.current_horizontal_shift}")
            track.write_to_screen()
            track.horizontal_shift_one()
        if virtual_screen:
            screen.show()
        time.sleep(0.2)

def show_usage():
    print("Usage:", sys.argv[0], " Optional: [--virtual <screen_height> <screen_width>]")

if __name__ == '__main__':
    if "-h" in sys.argv or "--help" in sys.argv:
        show_usage()
        exit(0)
    test_virtual_screen = False
    screen_height = 0
    screen_width = 0
    if "--virtual" in sys.argv:
        index = sys.argv.index("--virtual")
        if len(sys.argv) < 4 or len(sys.argv[index::]) < 3:
            show_usage()
            exit(1)
        test_virtual_screen = True
        screen_height = int(sys.argv[index + 1])
        screen_width = int(sys.argv[index + 2])
        
    test_track(test_virtual_screen, screen_height, screen_width)
