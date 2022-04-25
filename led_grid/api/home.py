import flask
import led_grid

@led_grid.app.route("/api/v1/home/hello_there/", methods=[ "GET" ])
def check():
    return flask.jsonify({ "status": 200, "message": "General Kenobi" })
    
@led_grid.app.route("/api/v1/home/toggle/", methods=[ "POST" ])
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

@led_grid.app.route("/api/v1/home/set_context/<name>/", methods=[ "POST" ])
def set_context(name):
    global check_bg_color
    if name == "clock":
        new_context = led_grid.hw.contexts.Clock(led_grid.screen)
    if name == "blank":
        new_context = led_grid.hw.contexts.Blank(led_grid.screen, (0, 0, 0))
    led_grid.hw.contexts.Context.set_context(new_context)
    
    return flask.jsonify({ "status": 200, "context": name })
    
@led_grid.app.route("/api/v1/home/active_context/", methods=[ "GET" ])
def active_context():
    active_context = led_grid.hw.contexts.Context.active_context
    if active_context is None:
        context_name = "No active context"
        context_desc = "No context is currently running"
    else:
        context_name = active_context.name()
        context_desc = active_context.desc()
    return flask.jsonify({
                           "status": 200,
                           "context": context_name,
                           "description": context_desc
                         })
    

