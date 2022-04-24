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
        self.switch_context = False
        self.switch_ready = False
        change = True
        screen_height = self.screen.height()
        screen_width =  self.screen.width()
        second = None
        prev_second = None

        self.switch_ready = False
        while not self.switch_context:
            current = datetime.datetime.now()
            current_hr = current.strftime("%H")
            current_min = current.strftime("%M")

            if change:
                current_hr = current_hr + ":"
            change = not change
            self.scene.clear_tracks(0)
            scene.add_text_to_track(current_hr, 0)        

            if current_min != scene.track_value(1):
                self.scene.clear_tracks(1)
                self.scene.add_text_to_track(current_min, 1, (233, 75, 60))        

            self.scene.frame()
            time.sleep(1)
        self.ready_switch = True

 
    def desc(self):
        return "Clock"
