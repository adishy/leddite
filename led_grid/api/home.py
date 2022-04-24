import flask
import led_grid

@led_grid.app.route("/api/v1/home/hello_there", methods=[ "GET" ])
def check():
    return flask.jsonify({ "status": 200, "message": "General Kenobi" })
    
@led_grid.app.route("/api/v1/home/toggle", methods=[ "GET" ])
def toggle():
    active_context = led_grid.hw.contexts.Context.active_context
    if active_context is None:
        context_to_set = led_grid.hw.contexts.Blank(led_grid.screen, (255, 255, 255))
    # If the screen is running the blank, restore the previous context
    elif active_context.__class__.__name__ == "Blank":
        prev_context = led_grid.hw.contexts.Context.prev_context
        if prev_context is None:
            return flask.jsonify({ "status": 200, "message": f"Current context is {active_context.desc()}, no prev context" })
        else:
            context_to_set = prev_context
    else:
       context_to_set = led_grid.hw.contexts.Blank(led_grid.screen) 
    led_grid.hw.contexts.Context.set_context(context_to_set)
    return flask.jsonify({ "status": 200, "message": f"Context switched to: {context_to_set.desc()}" })
