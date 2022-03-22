from track import Track

screen = [
           [ (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0) ],
           [ (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0) ],
           [ (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0) ],
           [ (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0) ],
           [ (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0) ],
           [ (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0) ],
           [ (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0) ],
           [ (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0) ],
         ]

def test_content(height, width, excess):
    color = (255, 255, 255)
    content = []
    for i in range(width + excess):
        for j in range(height):
            content.append((255, i % 256, j % 256))
    return content

def show():
    for i in screen:
        print(i)

def calculate_tracks(screen_height, screen_width, content_height, spacing):
    assert(content_height < screen_height)
    tracks = []
    track_count = int(screen_height / (content_height + spacing))
    horizontal_shift = spacing
    for i in range(track_count):
        tracks.append(Track(content_height, screen_width, 0, horizontal_shift))
        horizontal_shift += spacing + content_height
    return tracks

def create_and_test_tracks(screen)
    screen_height = len(screen)
    screen_width =  len(screen[0])
    font_height = font_med["height"]
    spacing = 1                     
    out_of_screen_cols = 12

    tracks = calculate_tracks(screen_height,
                              screen_width,
                              font_height,
                              spacing)
    for track in tracks:
        track.add_content(test_content(font_height, screen_width, out_of_screen_cols))
        
    for i in range(28):
        for i, track in enumerate(tracks):
            print(f"Track {i}: Current Shift: {track.current_horizontal_shift}")
            track.write_to_screen()
            track.horizontal_shift_one()
        show()
        print("\n")
        sleep(200)

if __name__ == '__main__':
    create_and_test_tracks()
