from threading import Thread, Event
from leddite.hw.contexts.context import Context
import time
import sys

class Carousel:
    active_context_ids = [ "welcome", "ip" ]
    carousel_stopped = True
    carousel_context = None
    carousel_thread = None
    carousel_switch_interval_sec = 5
    stop_carousel_event = Event()

    @classmethod
    def start(cls, blank_context):
        Context.set_context(blank_context)
        if cls.carousel_thread is not None:
            print("Carousel has already started", file=sys.stderr)
            return False
        cls.carousel_thread = Thread(target=cls.__run_carousel, args=(blank_context,), name=f"carousel_{int(time.time())}")
        cls.carousel_thread.start()
        cls.carousel_stopped = False
        return True

    @classmethod
    def __run_carousel(cls, blank_context):
        i = 0
        while not cls.stop_carousel_event.is_set():
           Context.set_context(blank_context)
           while Context.active_context_thread.is_alive():
             time.sleep(0.2)
           num_carousel_contexts = len(cls.active_context_ids)
           context_to_set = Context.context_registry[cls.active_context_ids[i % num_carousel_contexts]]
           Context.set_context(context_to_set)
           cls.carousel_context = context_to_set
           start_time = int(time.time())
           current_time = int(time.time())
           while (current_time - start_time) <= cls.carousel_context.interval_sec():
              current_time = int(time.time())
              if cls.stop_carousel_event.is_set():
                 return True
              else:
                 time.sleep(0.2)
           i += 1
        return True
    
    @classmethod
    def stop(cls, blank_context):
        if cls.carousel_thread is None:
            print("Carousel has not started", file=sys.stderr)
            return False
        cls.stop_carousel_event.set()
        while cls.carousel_thread.is_alive():
            print("Waiting for carousel thread to return", file=sys.stderr)
            time.sleep(0.1)
        cls.carousel_context = None
        cls.carousel_thread = None
        cls.stop_carousel_event.clear()
        Context.set_context(blank_context)
        cls.carousel_stopped = True
        return True

    @classmethod
    def info(cls):
        return {
                 "carousel_thread_name": cls.carousel_thread.name if cls.carousel_thread else None,
                 "carousel_stopped": cls.carousel_stopped,
                 "carousel_context": cls.carousel_context.name() if cls.carousel_thread else None,
                 "active_context_ids": cls.active_context_ids,
                 "contexts": { 
                     key: { **value.to_dict(), **{ "active": key in cls.active_context_ids } } \
                     for key, value in Context.context_registry.items()
                 },
                 "status": 200,
               }
