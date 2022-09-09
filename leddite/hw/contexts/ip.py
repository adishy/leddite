from leddite.hw.layouts.track import Track
from leddite.hw.contexts.context import Context
from leddite.hw.fonts.font_med import FontMed
from leddite.hw.fonts.font_small import FontSmall
from leddite.hw.layouts.scene import TextOnlyScene
import datetime
import sys
import socket
import time

class IP(Context):
    def __init__(self, screen):
        super().__init__(screen)
        self.small_text_scene= TextOnlyScene(screen=self.screen, inter_track_space=1, font=FontSmall)
        self.small_text_scene.generate_tracks()
        for track in self.small_text_scene.tracks:
            track.horizontal_shift = 2

    def get_local_ip(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8",80))
        ip_address = s.getsockname()[0]
        s.close()
        return ip_address

    def show(self):
        while self.screen.permission():
            message = "VISIT THE URL BELOW:"
            ip = self.get_local_ip()
            self.small_text_scene.clear_tracks()
            self.small_text_scene.add_text_to_track(message, 0,(233, 75, 60))
            self.small_text_scene.add_text_to_track(ip, 1, (255, 255, 255))
            self.small_text_scene.frame()

    def name(self):
        return "IP"
 
    def desc(self):
        return "Displays local IP Address"
    
    def interval_sec(self):
        return 10