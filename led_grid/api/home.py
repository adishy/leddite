import flask
import re
import threading
import led_grid

@led_grid.app.route("/api/v1/home/hello_there/", methods=[ "GET" ])
def check():
    return flask.jsonify({ "status": 200, "message": "General Kenobi" })
    
@led_grid.app.route("/api/v1/home/set_context/<name>/", methods=[ "POST" ])
def set_context(name):
    if led_grid.hw.contexts.Context.carousel_thread is not None:
        return flask.jsonify({ "status": 403, "error": "Carousel is active. Please stop carousel before manually setting context" })
    if name in led_grid.hw.contexts.Context.context_registry:
        new_context = led_grid.hw.contexts.Context.context_registry[name]
        if name in led_grid.hw.contexts.Context.context_handlers:
            context_handler = led_grid.hw.contexts.Context.context_handlers[name]
            context_handler(new_context)
    else:
        return flask.jsonify({ "status": 403, "error": "Could not find specified context by the context name" })
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
    
@led_grid.app.route("/api/v1/home/context_threads/", methods=[ "GET" ])
def active_context_threads():
    active_context_thread_uid = "No active context thread"
    active_context = led_grid.hw.contexts.Context.active_context
    if active_context:
        active_context_thread_uid = active_context.thread_uid()
    return flask.jsonify({
                           "screen_active_context_thread": led_grid.hw.screens.Screen.thread_uid,
                           "active_context_thread_name": active_context_thread_uid,
                           "all_threads": [ thread.name for thread in threading.enumerate() ],
                           "status": 200,
                         })

@led_grid.app.route("/api/v1/home/start_carousel/", methods=[ "POST" ])
def start_carousel():
    blank_context = led_grid.hw.contexts.Blank(led_grid.screen)
    contexts = flask.request.args.get("contexts")
    if contexts is not None:
        contexts = contexts.split(",")
    else:
        contexts = [ "clock", "calendar", "weather", "heartbeat" ]
    if led_grid.hw.contexts.Context.start_carousel(blank_context, contexts):
        return flask.jsonify({
                               "status": 200,
                               "msg": f"Carousel will be started with the following contexts: {contexts}"
                             })
    else:   
        return flask.jsonify({
                               "status": 403,
                               "msg": f"Carousel has already been started"
                             })

@led_grid.app.route("/api/v1/home/stop_carousel/", methods=[ "POST" ])
def stop_carousel():
    blank_context = led_grid.hw.contexts.Blank(led_grid.screen)
    if led_grid.hw.contexts.Context.stop_carousel(blank_context):
        return flask.jsonify({
                               "status": 200,
                               "msg": f"Carousel will be stopped"
                             })
    else:
        return flask.jsonify({
                               "status": 403,
                               "msg": f"Carousel is not running"
                             })

@led_grid.app.route("/api/v1/home/context_carousel/", methods=[ "GET" ])
def context_carousel():
    carousel_thread_uid = led_grid.hw.contexts.Context.carousel_thread
    if carousel_thread_uid is None:
        carousel_thread_uid = "No carousel thread"
        carousel_context = "No carousel context"
    else:
        carousel_thread_uid = carousel_thread_uid.name
        carousel_context = led_grid.hw.contexts.Context.carousel_context.name()
    return flask.jsonify({
                           "carousel_thread_name": carousel_thread_uid,
                           "carousel_context": carousel_context,
                           "status": 200,
                         })
 
