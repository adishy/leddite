import flask
import threading
import leddite

def blank_context_handler(context):
    color = (0, 0, 0)
    bg = flask.request.args.get("bg")
    if bg is not None and bool(re.match(r"\(\d{1,3}\,\d{1,3},\d{1,3}\)", bg)):
        r,g,b = [ int(color) for color in bg.replace(")", "").replace("(", "").split(",") ]
        if (r >= 0 and r < 256) and (g >= 0 and g < 256) and (b >= 0 and b < 256):
            color = (r, g, b)
        context.background_color = color 

context_request_handlers = {
    "blank": blank_context_handler
}

@leddite.app.route("/api/v1/context/set/<name>/", methods=[ "POST" ])
def context_set(name):
    if leddite.hw.contexts.Context.carousel_thread is not None:
        return flask.jsonify({ "status": 403, "error": "Carousel is active. Please stop carousel before manually setting context" })
    if name in leddite.hw.contexts.Context.context_registry:
        new_context = leddite.hw.contexts.Context.context_registry[name]
        if name in leddite.hw.contexts.Context.context_handlers:
            context_handler = leddite.hw.contexts.Context.context_handlers[name]
            context_handler(new_context)
    else:
        return flask.jsonify({ "status": 403, "error": "Could not find specified context by the context name" })
    leddite.hw.contexts.Context.set_context(new_context)
    return flask.jsonify({ "status": 200, "context": name })
    
@leddite.app.route("/api/v1/context/info/", methods=[ "GET" ])
def context_info():
    active_context = leddite.hw.contexts.Context.active_context
    return flask.jsonify({
                           "screen_active_context_thread": leddite.hw.screens.Screen.thread_uid,
                           "active_context_thread_name": active_context.thread_uid() if active_context else None,
                           "active_context_name": active_context.name() if active_context else None,
                           "active_context_description": active_context.desc() if active_context else None,
                           "all_threads": [ thread.name for thread in threading.enumerate() ],
                           "status": 200,
                         })


