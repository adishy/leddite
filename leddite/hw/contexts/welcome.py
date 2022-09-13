from leddite.hw.layouts.track import Track
from leddite.hw.contexts.context import Context
from leddite.hw.layouts.scene import TextOnlyScene
from leddite.hw.fonts.font_med import FontMed
from leddite.hw.fonts.font_small import FontSmall
from leddite.hw.fonts.glyph import HeartSymbolSmaller
from os import environ as env
import sys
import time

class Welcome(Context):
    def __init__(self, screen):
        super().__init__(screen)
        self.scene = TextOnlyScene(screen=self.screen, inter_track_space=1, font=FontSmall)
        self.scene.generate_tracks()
        self.scene.add_text_to_track("HAPPY", 0) 
        self.scene.add_text_to_track("BDAY ", 1, (233, 75, 60)) 
        self.scene.add_text_to_track("BOO!", 2, (255, 0, 0))

    def show(self):
        while self.screen.permission():
            self.scene.frame()
            time.sleep(0.05)

    def name(self):
        return "Welcome"
 
    def desc(self):
        return "Displays a message"

    def interval_sec(self):
        return 15

    def meta_context(self):
        return False

