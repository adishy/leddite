"""leddite package initializer."""
from threading import Thread, Lock, Condition
import flask
import sys

app = flask.Flask(__name__, static_url_path='/static')  # pylint: disable=invalid-name
app.config.from_envvar('LED_GRID_SETTINGS', silent=True)

import leddite.views  # noqa: E402  pylint: disable=wrong-import-position
import leddite.api    # noqa: E402  pylint: disable=wrong-import-position 
import leddite.hw     # noqa: E402  pylint: disable=wrong-import-position  

shut_down = False
screen_thread = None
screen = None


def initialize_context_registry(screen):
    def blank_context_handler(context):
        color = (0, 0, 0)
        bg = flask.request.args.get("bg")
        if bg is not None and bool(re.match(r"\(\d{1,3}\,\d{1,3},\d{1,3}\)", bg)):
            r,g,b = [ int(color) for color in bg.replace(")", "").replace("(", "").split(",") ]
            if (r >= 0 and r < 256) and (g >= 0 and g < 256) and (b >= 0 and b < 256):
                color = (r, g, b)
            context.background_color = color 
    
    contexts_available = [
                          { "context": leddite.hw.contexts.Clock(screen), "handler": None },
                          { "context": leddite.hw.contexts.Weather(screen), "handler": None },
                          { "context": leddite.hw.contexts.Blank(screen), "handler": blank_context_handler },
                          { "context": leddite.hw.contexts.Calendar(screen), "handler": None },
                          { "context": leddite.hw.contexts.Heartbeat(screen), "handler": None },
                         ]
    
    for context_data in contexts_available:
        context = context_data["context"]
        name = context.name()
        leddite.hw.contexts.Context.context_registry[name] = context
        if context_data["handler"] is not None:
            leddite.hw.contexts.Context.context_handlers[name] = context_data["handler"]
   
def run(port, virtual=True, h=16, w=16):
    if virtual:
        leddite.screen = leddite.hw.screens.VirtualScreen(h, w)
    else:
        leddite.screen = leddite.hw.screens.PhysicalScreen()
    initialize_context_registry(leddite.screen)
    app.run(debug=True, host='0.0.0.0', port=port, use_reloader=False)
   
