from leddite.hw.layouts.track import Track
from leddite.hw.contexts.context import Context
from leddite.hw.layouts.scene import TextOnlyScene
from leddite.hw.fonts.font_med import FontMed
from leddite.hw.fonts.font_small import FontSmall
from dotenv import load_dotenv
from os import environ as env
import requests
import json
import datetime
import sys
import time

class Weather(Context):
    def __init__(self, screen):
        super().__init__(screen)
        self.scene = TextOnlyScene(screen=self.screen, inter_track_space=1)
        self.scene.generate_tracks()
        self.scene.set_font_for_track(1, FontSmall)
        self.scene.tracks[1].vertical_shift += 1
        self.scene.tracks[0].do_not_scroll = True
        load_dotenv()
        self.api_base_url = "https://api.openweathermap.org/data/2.5/weather"
        self.args = {
            "units": "metric",
            "lat": env["OPENWEATHERMAP_API_LAT"],
            "lon": env["OPENWEATHERMAP_API_LONG"],
            "appid": env["OPENWEATHERMAP_API_SECRET"]
        }
        self.last_update = int(time.time())
        self.update_interval_sec = 600
        self.weather_data = None
        construct_endpoint = requests.models.PreparedRequest()
        construct_endpoint.prepare_url(self.api_base_url, self.args)
        self.weather_data_endpoint = construct_endpoint.url

    def retrieve_weather(self):
        default = {
           "main": { "feels_like": "ERR" },
           "weather": { "main": "ERR" }
        }
        api_response = requests.get(self.weather_data_endpoint)
        if api_response.status_code != 200:
             print(api_response.text, file=sys.stderr)
             return default
        else:
             return api_response.json() 

    def update(self, override=False):
        current = int(time.time())
        if override or self.weather_data is None or (current - self.last_update >= self.update_interval_sec):
            self.last_update = current
            self.weather_data = self.retrieve_weather() 
            print(json.dumps(self.weather_data, indent=5), file=sys.stderr)
            print("Updated weather data", file=sys.stderr)
            return True
        return False
   
    def write_weather_data_to_tracks(self): 
        self.update()
        temp = self.weather_data["main"]["feels_like"]
        desc = self.weather_data["weather"][0]["main"].upper()
        self.scene.clear_tracks()
        self.scene.add_text_to_track(f"{int(temp)}°", 0)
        self.scene.add_text_to_track(desc, 1, (233, 75, 60))

    def show(self):
        while self.screen.permission():
            self.write_weather_data_to_tracks()
            self.scene.frame()
            time.sleep(0.15)

    def name(self):
        return "Weather"
 
    def desc(self):
        return "Displays the current 'Feels Like' temperature and conditions for a specified location"

    def interval_sec(self):
        return 3

    def meta_context(self):
        return False
