import flask
import threading
import leddite

@leddite.app.route("/api/v1/carousel/start/", methods=[ "POST" ])
def carousel_start():
    blank_context = leddite.hw.contexts.Blank(leddite.screen)
    contexts = flask.request.args.get("contexts")
    if contexts is not None:
        contexts = contexts.split(",")
    else:
        contexts = [ "clock", "calendar", "weather", "heartbeat" ]
    if leddite.hw.contexts.Context.start_carousel(blank_context, contexts):
        return flask.jsonify({
                               "carousel_stopped": False,
                               "status": 200,
                               "msg": f"Carousel will be started with the following contexts: {contexts}"
                             })
    else:   
        return flask.jsonify({
                               "carousel_stopped": False,
                               "status": 403,
                               "msg": f"Carousel has already been started"
                             })

@leddite.app.route("/api/v1/carousel/stop/", methods=[ "POST" ])
def carousel_stop():
    blank_context = leddite.hw.contexts.Blank(leddite.screen)
    if leddite.hw.contexts.Context.stop_carousel(blank_context):
        return flask.jsonify({
                               "carousel_stopped": True,
                               "status": 200,
                               "msg": f"Carousel will be stopped"
                             })
    else:
        return flask.jsonify({
                               "carousel_stopped": True,
                               "status": 403,
                               "msg": f"Carousel is not running"
                             })

@leddite.app.route("/api/v1/carousel/info/", methods=[ "GET" ])
def context_carousel():
    carousel_thread_uid = leddite.hw.contexts.Context.carousel_thread
    if carousel_thread_uid is None:
        carousel_thread_name= None
        carousel_context = None
    else:
        carousel_thread_name = carousel_thread_uid.name
        carousel_context = leddite.hw.contexts.Context.carousel_context.name()
    return flask.jsonify({
                           "carousel_thread_name": carousel_thread_name,
                           "carousel_stopped": carousel_thread_uid is None,
                           "carousel_context": carousel_context,
                           "status": 200,
                         })
 

