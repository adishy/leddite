"""led_grid package initializer."""
from threading import Thread, Lock, Condition
import logging
import flask
import sys

app = flask.Flask(__name__, static_url_path='/static')  # pylint: disable=invalid-name
app.config.from_envvar('LED_GRID_SETTINGS', silent=True)

import led_grid.views  # noqa: E402  pylint: disable=wrong-import-position
import led_grid.api    # noqa: E402  pylint: disable=wrong-import-position 
import led_grid.hw     # noqa: E402  pylint: disable=wrong-import-position  

shut_down = False
screen_thread = None
screen = None

@app.before_first_request
def initialize():
    logger = logging.getLogger("led_grid")
    logger.setLevel(logging.DEBUG)
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    formatter = logging.Formatter(
    """%(levelname)s in %(module)s [%(pathname)s:%(lineno)d]:\n%(message)s"""
    )
    ch.setFormatter(formatter)
    logger.addHandler(ch)

def show():
    led_grid.screen.refresh()
    with led_grid.screen.cv:
        led_grid.screen.cv.wait_for(led_grid.screen.changed)
        led_grid.screen.refresh()
    #while True:
    #  if shut_down:
    #      sys.exit(0)
    #  if led_grid.screen.changed():
    #     led_grid.screen.refresh()
    
def run(port, virtual=True, h=16, w=16):
    if virtual:
        led_grid.screen = led_grid.hw.screens.VirtualScreen(h, w)
    else:
        led_grid.screen = led_grid.hw.screens.PhysicalScreen()
    led_grid.screen_thread = Thread(target=show)
    led_grid.screen_thread.start()
    app.run(debug=True, port=port, use_reloader=False)
   
