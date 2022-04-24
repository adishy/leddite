from led_grid.hw.layouts.track import Track
from led_grid.hw.fonts.font_med import FontMed
from led_grid.hw.screens import *
import sys
import time

class Scene:
    def __init__(self, **kwargs):
       '''
        screen (reqd): Class Screen
        content_height (reqd): int, max height of content 
        inter_track_space (reqd): int, space between each track
        track_count: int, desired numebr of tracks on screen
       '''
       self.screen = kwargs["screen"]
       self.tracks = []        
       self.inter_frame_pause_sec = 0.27
       if "inter_frame_pause_sec" in kwargs:
           self.inter_frame_pause_sec = kwargs["inter_frame_pause_sec"]
       self.content_height = kwargs["content_height"]
       self.inter_track_space = kwargs["inter_track_space"]
       self.track_count = -1
       if "track_count" in kwargs:
         self.track_count = kwargs["track_count"] 
    
    def max_tracks(self):        
        screen_height = self.screen.height()
        screen_width = self.screen.width()
        assert(self.content_height < screen_height)
        track_count = int(screen_height / (self.content_height + self.inter_track_space))
        return track_count

    def generate_tracks(self, track_count=-1): 
        max_tracks = self.max_tracks()
        if self.track_count < 0 or self.track_count > max_tracks:
           self.track_count = max_tracks 
        vertical_offset = self.inter_track_space
        for i in range(self.track_count):
            self.tracks.append(Track(self.screen, self.content_height, self.screen.width(), 0, vertical_offset, self.screen.color((0, 0, 0))))
            vertical_offset += self.inter_track_space + self.content_height

    def frame(self):
       for i, track in enumerate(self.tracks):
           empty = track.empty()
           if empty:
             continue
           if track.write_to_screen():
              print(f"Track {i}: Current Shift: {track.current_horizontal_shift}, Content width: {track.get_contents_width()}")
           if track.get_contents_width() > self.screen.width():
               track.horizontal_shift_one()
       time.sleep(self.inter_frame_pause_sec)

class TextOnlyScene(Scene):
    def __init__(self, **kwargs):
        '''
         Scene:
           screen (reqd): Class Screen
           content_height (not reqd when initializing TextOnlyScene): int, max height of content 
           inter_track_space (reqd): int, space between each track
           track_count: int, desired numebr of tracks on screen
           inter_frame_pause_sec: Time in s between each frame
        
         TextOnlyScene:
           font (reqd): Font dictionary to use
           wrap_around_space: Space between end and start of text when text-wrapping content
        '''
        s_args = kwargs
        self.track_values = []
        self.wrap_around_space = 3
        if "wrap_around_space" in kwargs:
            self.wrap_around_space = kwargs["wrap_around_space"]
        self.font = FontMed
        if "font" in kwargs:
            self.font = kwargs["font"]
        s_args["content_height"] = self.font["height"]
        super().__init__(**s_args)

    def generate_tracks(self, track_count=-1):
        super().generate_tracks(track_count)
        self.track_values = [ "" for _ in range(len(self.tracks)) ]
        
        
    def generate_text(self, value, color=(255,255,255), font=None):
        if font is None:
            font = font_med
        content = []
        for symbol in value:
            glyph = self.glyph_to_col_major(font["symbols"][symbol], font)
            content_glyph = []
            for pos in glyph:
                if not pos:
                   content_glyph.append(self.screen.color((0, 0, 0)))
                else:
                   content_glyph.append(self.screen.color(color))
            content += content_glyph
            content += [ self.screen.color((0, 0, 0)) for _ in range(font["height"]) ]
        for _ in range(self.wrap_around_space):
            content += [ self.screen.color((0, 0, 0)) for _ in range(font["height"]) ]
        return content

    def add_text_to_track(self, value, track_id, color=(255,255,255)):
        if not self.tracks:
            self.generate_tracks()
        assert(track_id >= 0 and track_id < self.max_tracks())
        assert(track_id < len(self.tracks))
        self.tracks[track_id].add_content(self.generate_text(value, color))
        self.track_values[track_id] += value

    def clear_tracks(self, id=-1):
        if id >= len(self.tracks) or id < 0:
            for track in self.tracks:
                track.clear()
        else:
            self.tracks[id].clear()
            self.track_values[id] = ""

    def track_value(self, id):
        if not self.tracks:
            self.generate_tracks()
        return self.track_values[id]

    def glyph_to_col_major(self, glyph, font):
        col_major_glyph = []
        height = font["height"]
        width = font["width"]
        for x in range(width):
            col_major_glyph += [ glyph[ y * width + x ] for y in range(height) ]
        return col_major_glyph

def main(screen):
    screen_height = screen.height()
    screen_width =  screen.width()
    scene = TextOnlyScene(screen=screen, inter_track_space=1)
    scene.add_text_to_track("HI", 0)
    scene.add_text_to_track("HELLO THERE", 1, (0, 255, 255))
    
    while True:
        scene.frame()

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

