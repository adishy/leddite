from led_grid.hw.layouts.track import Track
from led_grid.hw.contexts.context import Context
from led_grid.hw.layouts.scene import TextOnlyScene
import datetime
import sys
import time

class Clock(Context):
    def __init__(self, screen):
        super().__init__(screen)
        self.scene = TextOnlyScene(screen=self.screen, inter_track_space=1)
        self.scene.generate_tracks()
        self.scene.tracks[0].do_not_scroll = True

    def show(self):
        change = True
        screen_height = self.screen.height()
        screen_width =  self.screen.width()
        second = None
        prev_second = None

        while self.screen.permission():
            current = datetime.datetime.now()
            current_hr = current.strftime("%H")
            current_min = current.strftime("%M")

            if change:
                current_hr = current_hr + ":"
            change = not change
            self.scene.clear_tracks(0)
            self.scene.add_text_to_track(current_hr, 0)        

            if current_min != self.scene.track_value(1):
                self.scene.clear_tracks(1)
                self.scene.add_text_to_track(current_min, 1, (233, 75, 60))        
            self.scene.frame()
            time.sleep(1)
        print("current context is changed: clock")

    def name(self):
        return "clock"
 
    def desc(self):
        return "Displays a clock"
