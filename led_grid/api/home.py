from flask import *
import led_grid

@led_grid.app.route("/api/v1/home/toggle", methods=[ "POST" ])
def toggle():
    context = led_grid.hw.scenes.blank
    # If the screen is blank, restore the previous context
    if led_grid.current_context == led_grid.hw.contexts.blank:
        assert(led_grid.prev_context is not None)
        context = led_grid.prev_context
    # Save the current context
    led_grid.prev_context = led_grid.current_context
    context.show()
    switch(led_grid.screen)
    return flask.jsonify({ "status": 200, "message": f"Context switched to: {context.desc()}" })
