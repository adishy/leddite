"""leddite package initializer."""
from threading import Thread, Lock, Condition
import flask
import sys

app = flask.Flask(__name__, static_url_path='/static')  # pylint: disable=invalid-name
app.config.from_envvar('LED_GRID_SETTINGS', silent=True)

import leddite.views               # noqa: E402  pylint: disable=wrong-import-position
import leddite.api                 # noqa: E402  pylint: disable=wrong-import-position 
import leddite.hw                  # noqa: E402  pylint: disable=wrong-import-position  
from leddite.cli import run_cli    # noqa: E402  pylint: disable=wrong-import-position 

shut_down = False
screen_thread = None
screen = None

def run(port, virtual=True, h=16, w=16):
    if virtual:
        leddite.screen = leddite.hw.screens.VirtualScreen(h, w)
    else:
        leddite.screen = leddite.hw.screens.PhysicalScreen()
    leddite.hw.contexts.initialize_registry(leddite.screen)
    blank_context = leddite.hw.contexts.Blank(leddite.screen)
    leddite.hw.contexts.Carousel.start(blank_context)
    serve(port)

def serve(port):
    app.run(debug=True, host='0.0.0.0', port=port, use_reloader=True)


