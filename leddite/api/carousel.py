import flask
import threading
import leddite

@leddite.app.route("/api/v1/carousel/start/", methods=[ "POST" ])
def carousel_start():
    blank_context = leddite.hw.contexts.Blank(leddite.screen)
    contexts = leddite.hw.contexts.Carousel.active_context_ids
    if leddite.hw.contexts.Carousel.start(blank_context, contexts):
        return flask.jsonify({
                               "carousel_stopped": False,
                               "status": 200,
                               "active_context_ids": contexts,
                               "msg": f"Carousel will be started!"
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
    if leddite.hw.contexts.Carousel.stop(blank_context):
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

@leddite.app.route("/api/v1/carousel/contexts/update/", methods=[ "POST" ])
def carousel_contexts_update():
    context_ids = flask.get("context_ids")
    contexts_to_set = []
    print(context_ids)
    for context in context_ids:
       if context not in leddite.hw.contexts.Context.context_registry:
            continue
       contexts_to_set.append(context)
    leddite.hw.contexts.Carousel.active_context_ids = contexts_to_set
    return flask.jsonify(leddite.hw.contexts.Carousel.info())

@leddite.app.route("/api/v1/carousel/info/", methods=[ "GET" ])
def carousel_info():
    return flask.jsonify(leddite.hw.contexts.Carousel.info())
