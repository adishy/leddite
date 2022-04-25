"""led_grid package initializer."""
from threading import Thread, Lock, Condition
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

def run(port, virtual=True, h=16, w=16):
    if virtual:
        led_grid.screen = led_grid.hw.screens.VirtualScreen(h, w)
    else:
        led_grid.screen = led_grid.hw.screens.PhysicalScreen()
    #led_grid.screen_thread = Thread(target=show)
    #led_grid.screen_thread.start()
    app.run(debug=True, port=port, use_reloader=False)
   
