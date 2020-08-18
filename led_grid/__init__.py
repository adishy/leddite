"""led_grid package initializer."""
import flask

app = flask.Flask(__name__)  # pylint: disable=invalid-name
app.config.from_envvar('LED_GRID_SETTINGS', silent=True)

import led_grid.views  # noqa: E402  pylint: disable=wrong-import-position

from led_grid.hw import Screen

screen = Screen()
