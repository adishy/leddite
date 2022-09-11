from leddite.hw.contexts.blank import Blank
from leddite.hw.contexts.calendar import Calendar
from leddite.hw.contexts.carousel import Carousel
from leddite.hw.contexts.clock import Clock
from leddite.hw.contexts.context import Context
from leddite.hw.contexts.heartbeat import Heartbeat
from leddite.hw.contexts.ip import IP
from leddite.hw.contexts.weather import Weather
from leddite.hw.contexts.welcome import Welcome

def initialize_registry(screen):
    import leddite
    contexts_available = [
                            leddite.hw.contexts.blank.Blank(screen),
                            leddite.hw.contexts.calendar.Calendar(screen),
                            leddite.hw.contexts.clock.Clock(screen),
                            leddite.hw.contexts.heartbeat.Heartbeat(screen),
                            leddite.hw.contexts.ip.IP(screen),
                            leddite.hw.contexts.weather.Weather(screen),
                            leddite.hw.contexts.welcome.Welcome(screen),
                         ]
    
    for context in contexts_available:
        leddite.hw.contexts.Context.context_registry[context.id()] = context



