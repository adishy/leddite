import flask
import threading
import leddite

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
    
@leddite.app.route("/api/v1/context/active/", methods=[ "GET" ])
def context_active():
    active_context = leddite.hw.contexts.Context.active_context
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
    
@leddite.app.route("/api/v1/context/info/all/", methods=[ "GET" ])
def context_info_all():
    active_context_thread_uid = "No active context thread"
    active_context = leddite.hw.contexts.Context.active_context
    if active_context:
        active_context_thread_uid = active_context.thread_uid()
    return flask.jsonify({
                           "screen_active_context_thread": leddite.hw.screens.Screen.thread_uid,
                           "active_context_thread_name": active_context_thread_uid,
                           "all_threads": [ thread.name for thread in threading.enumerate() ],
                           "status": 200,
                         })


