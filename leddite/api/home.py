import flask
import re
import threading
import leddite

@leddite.app.route("/api/v1/home/hello_there/", methods=[ "GET" ])
def check():
    return flask.jsonify({ "status": 200, "message": "General Kenobi" })
    
@leddite.app.route("/api/v1/home/set_context/<name>/", methods=[ "POST" ])
def set_context(name):
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
    
@leddite.app.route("/api/v1/home/active_context/", methods=[ "GET" ])
def active_context():
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
    
@leddite.app.route("/api/v1/home/context_threads/", methods=[ "GET" ])
def active_context_threads():
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

@leddite.app.route("/api/v1/home/start_carousel/", methods=[ "POST" ])
def start_carousel():
    blank_context = leddite.hw.contexts.Blank(leddite.screen)
    contexts = flask.request.args.get("contexts")
    if contexts is not None:
        contexts = contexts.split(",")
    else:
        contexts = [ "clock", "calendar", "weather", "heartbeat" ]
    if leddite.hw.contexts.Context.start_carousel(blank_context, contexts):
        return flask.jsonify({
                               "status": 200,
                               "msg": f"Carousel will be started with the following contexts: {contexts}"
                             })
    else:   
        return flask.jsonify({
                               "status": 403,
                               "msg": f"Carousel has already been started"
                             })

@leddite.app.route("/api/v1/home/stop_carousel/", methods=[ "POST" ])
def stop_carousel():
    blank_context = leddite.hw.contexts.Blank(leddite.screen)
    if leddite.hw.contexts.Context.stop_carousel(blank_context):
        return flask.jsonify({
                               "status": 200,
                               "msg": f"Carousel will be stopped"
                             })
    else:
        return flask.jsonify({
                               "status": 403,
                               "msg": f"Carousel is not running"
                             })

@leddite.app.route("/api/v1/home/context_carousel/", methods=[ "GET" ])
def context_carousel():
    carousel_thread_uid = leddite.hw.contexts.Context.carousel_thread
    if carousel_thread_uid is None:
        carousel_thread_uid = "No carousel thread"
        carousel_context = "No carousel context"
    else:
        carousel_thread_uid = carousel_thread_uid.name
        carousel_context = leddite.hw.contexts.Context.carousel_context.name()
    return flask.jsonify({
                           "carousel_thread_name": carousel_thread_uid,
                           "carousel_context": carousel_context,
                           "status": 200,
                         })
 
