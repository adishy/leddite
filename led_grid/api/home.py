import flask
import re
import led_grid

@led_grid.app.route("/api/v1/home/hello_there/", methods=[ "GET" ])
def check():
    return flask.jsonify({ "status": 200, "message": "General Kenobi" })
    
@led_grid.app.route("/api/v1/home/set_context/<name>/", methods=[ "POST" ])
def set_context(name):
    if name == "clock":
        new_context = led_grid.hw.contexts.Clock(led_grid.screen)
    if name == "blank":
        color = (0, 0, 0)
        bg = flask.request.args.get("bg")
        if bg is not None and bool(re.match(r"\(\d{1,3}\,\d{1,3},\d{1,3}\)", bg)):
            r,g,b = [ int(color) for color in bg.replace(")", "").replace("(", "").split(",") ]
            if (r >= 0 and r < 256) and (g >= 0 and g < 256) and (b >= 0 and b < 256):
                color = (r, g, b)
        new_context = led_grid.hw.contexts.Blank(led_grid.screen, color)
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
    

