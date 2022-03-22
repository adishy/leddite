from track import Track
from screen import Screen
from font import font_med
from virtual_screen import VirtualScreen

def test_content(height, width, excess):
    color = (255, 255, 255)
    content = []
    for i in range(width + excess):
        for j in range(height):
            content.append((255, (i * 10) % 256, (j * 10) % 256))
    return content

def calculate_tracks(screen, content_height, spacing, mode="hw"):
    screen_height = screen.height()
    screen_width = screen.width()
    assert(content_height < screen_height)
    tracks = []
    track_count = int(screen_height / (content_height + spacing))
    horizontal_shift = spacing
    for i in range(track_count):
        tracks.append(Track(content_height, screen_width, 0, horizontal_shift, mode))
        horizontal_shift += spacing + content_height
    return tracks

def test_track(virtual_screen=False):
    if not virtual_screen:
        screen = Screen()
    else:
        screen = VirtualScreen()
    screen_height = screen.height()
    screen_width =  screen.width()
    font_height = font_med["height"]
    spacing = 1                     
    out_of_screen_cols = 12

    tracks = calculate_tracks(screen, font_height, spacing)
    for track in tracks:
        track.add_content(test_content(font_height, screen_width, out_of_screen_cols))
        
    for i in range(28):
        for i, track in enumerate(tracks):
            print(f"Track {i}: Current Shift: {track.current_horizontal_shift}")
            track.write_to_screen()
            track.horizontal_shift_one()
        if virtual_screen:
            screen.show()
        sleep(200)
    
if __name__ == '__main__':
    test_virtual_screen = False
    test_track(test_virtual_screen)
