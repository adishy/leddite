from leddite.hw.screens.screen import Screen
from threading import Thread, Lock, Event
import random
import time
import string
import sys

class Context:
    def __init__(self, screen):
        self.uid = ''.join(random.choices(string.ascii_uppercase + string.ascii_lowercase, k=5))
        self.screen = screen

    context_registry = {}
    carousel_context_registry = {}
    context_handlers = {}
    carousel_context = None
    carousel_thread = None
    carousel_switch_interval_sec = 5
    stop_carousel_event = Event()
    active_context = None
    active_context_thread = None

    @classmethod
    def start_carousel(cls, blank_context, carousel_context_names = [ "clock", "calendar" ]):
        cls.set_context(blank_context)
        if cls.carousel_thread is not None:
            print("Carousel has already started", file=sys.stderr)
            return False
        cls.carousel_thread = Thread(target=cls.__run_carousel, args=(blank_context, carousel_context_names), name=f"carousel_{int(time.time())}")
        cls.carousel_thread.start()
        return True

    @classmethod
    def __run_carousel(cls, blank_context, carousel_context_names = [ "clock", "calendar" ]):
        i = 0
        while not cls.stop_carousel_event.is_set():
           cls.set_context(blank_context)
           while cls.active_context_thread.is_alive():
             time.sleep(0.2)
           num_carousel_contexts = len(carousel_context_names)
           context_to_set = cls.context_registry[carousel_context_names[i % num_carousel_contexts]]
           cls.set_context(context_to_set)
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
    def stop_carousel(cls, blank_context):
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
        cls.set_context(blank_context)
        return True

    @classmethod
    def set_context(cls, new_context):
      cls.__set_new_context(new_context)

    @classmethod
    def __set_new_context(cls, new_context):
       Screen.switcher_lock.acquire()
       Screen.thread_id = new_context.thread_uid()
       cls.active_context = new_context
       cls.active_context_thread = Thread(target=cls.active_context.show, name=new_context.thread_uid())
       cls.active_context_thread.start()
       Screen.switcher_lock.release()
    
    @classmethod
    def __indicate(cls):
        cls.need_to_switch = True

    def show(self):
        pass

    def thread_uid(self):
        return f"{self.name()}_{self.uid}"

    def name(self):
        return "Default Context"
    
    def desc(self):
        return "This is the Default Context"

    def restore(self):
        pass

    def interval_sec(self):
        return Context.carousel_switch_interval_sec
