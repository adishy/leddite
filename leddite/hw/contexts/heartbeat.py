from leddite.hw.layouts.track import Track
from leddite.hw.contexts.context import Context
from leddite.hw.layouts.scene import TextOnlyScene
from leddite.hw.fonts import FontMed
from leddite.hw.fonts import FontSmall
from leddite.hw.fonts import HeartSymbolSmaller
from dotenv import load_dotenv
from os import environ as env
import requests
import json
import datetime
import sys
import time

class Heartbeat(Context):
    def __init__(self, screen):
        super().__init__(screen)
        self.scene = TextOnlyScene(screen=self.screen, inter_track_space=1)
        self.scene.generate_tracks()
        self.scene.set_font_for_track(0, HeartSymbolSmaller)
        self.scene.set_font_for_track(1, FontSmall)
        self.scene.tracks[0].do_not_scroll = True
        self.scene.tracks[0].vertical_shift -= 1
        self.scene.tracks[1].vertical_shift -= 4 
        self.scene.tracks[1].horizontal_shift += 4 
        self.scene.tracks[1].do_not_scroll = True
        
        load_dotenv()
        self.last_update = int(time.time())
        self.update_interval_sec = 120
        self.heartbeat_data = None
        self.heartbeat_data_endpoint = "https://heartbeat.home.adishy.com/get_last"

    def retrieve_heartbeat(self):
        default = {
           "heart_rate": 69
        }
        api_response = requests.get(self.heartbeat_data_endpoint)
        if api_response.status_code != 200:
             return default
        else:
             return json.loads(api_response.json())

    def update(self, override=False):
        current = int(time.time())
        if override or self.heartbeat_data is None or (current - self.last_update >= self.update_interval_sec):
            self.last_update = current
            self.heartbeat_data = self.retrieve_heartbeat() 
            print(json.dumps(self.heartbeat_data, indent=5), file=sys.stderr)
            print("Updated heartbeat data", file=sys.stderr)
            return True
        return False
   
    def write_heartbeat_data_to_tracks(self): 
        self.update()
        heart_rate = self.heartbeat_data["heart_rate"]
        self.scene.clear_tracks()
        self.scene.add_text_to_track("♥", 0, (228, 57, 52))
        self.scene.add_text_to_track(f"{heart_rate}", 1)

    def show(self):
        while self.screen.permission():
            self.write_heartbeat_data_to_tracks()
            self.scene.frame()
            time.sleep(0.15)

    def name(self):
        return "heartbeat"
 
    def desc(self):
        return "Displays Heartbeat using the HeartrateHeartbeat API"

    def interval_sec(self):
        return 2 

