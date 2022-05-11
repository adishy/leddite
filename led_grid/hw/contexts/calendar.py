from led_grid.hw.layouts.track import Track
from led_grid.hw.contexts.context import Context
from led_grid.hw.fonts.font_med import FontMed
from led_grid.hw.fonts.font_small import FontSmall
from led_grid.hw.layouts.scene import TextOnlyScene
import datetime
import sys
import time

class Calendar(Context):
    def __init__(self, screen):
        super().__init__(screen)
        self.small_text_scene= TextOnlyScene(screen=self.screen, inter_track_space=1, font=FontSmall)
        self.small_text_scene.generate_tracks()
        for track in self.small_text_scene.tracks:
            track.do_not_scroll = True
        for track in self.small_text_scene.tracks:
            track.horizontal_shift = 2

    def show(self):
        while self.screen.permission():
            weekday = datetime.datetime.now().strftime("%a").upper()
            day = datetime.datetime.now().strftime("%d")
            month_name = datetime.datetime.now().strftime("%b").upper()

            self.small_text_scene.clear_tracks()

            self.small_text_scene.add_text_to_track(day, 0, (255, 255, 255))
            self.small_text_scene.add_text_to_track(month_name, 1,(233, 75, 60))
            self.small_text_scene.add_text_to_track(weekday, 2, (233, 75, 68))

            self.small_text_scene.frame()

            time.sleep(1)

    def name(self):
        return "calendar"
 
    def desc(self):
        return "Displays a calendar"
    
    def interval_sec(self):
        return 2
