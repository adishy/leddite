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
        self.small_text_scene= TextOnlyScene(screen=self.screen, inter_track_space=0, font=FontSmall)
        self.med_text_scene= TextOnlyScene(screen=self.screen, inter_track_space=0, font=FontMed)
        self.small_text_scene.generate_tracks()
        self.med_text_scene.generate_tracks()
        for track in self.small_text_scene.tracks:
            track.do_not_scroll = True
        for track in self.med_text_scene.tracks:
            track.do_not_scroll = True
        self.med_text_scene.tracks[0].horizontal_shift = 2 
        self.small_text_scene.tracks[2].horizontal_shift = 2
        self.small_text_scene.tracks[2].vertical_shift -= 1
        self.small_text_scene.tracks[3].horizontal_shift = 2

    def show(self):
        while self.screen.permission():
            weekday = datetime.datetime.now().strftime("%a").upper()
            day = datetime.datetime.now().strftime("%d")
            month_name = datetime.datetime.now().strftime("%b").upper()

            self.small_text_scene.clear_tracks()
            self.med_text_scene.clear_tracks()

            self.med_text_scene.add_text_to_track(day, 0, (255, 255, 255), FontMed)
            self.small_text_scene.add_text_to_track(month_name, 2,  (233, 75, 60), FontSmall)
            self.small_text_scene.add_text_to_track(weekday, 3, (233, 82, 128), FontSmall)

            self.med_text_scene.frame()
            self.small_text_scene.frame()

            time.sleep(1)

    def name(self):
        return "calendar"
 
    def desc(self):
        return "Displays a calendar"
