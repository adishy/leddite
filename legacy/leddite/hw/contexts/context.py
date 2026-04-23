from leddite.hw.screens.screen import Screen
from json import JSONEncoder
from threading import Thread, Lock, Event
import random
import time
import string
import sys

def wrapped_default(self, obj):
    return getattr(obj.__class__, "__json__", wrapped_default.default)(obj)
wrapped_default.default = JSONEncoder().default
JSONEncoder.original_default = JSONEncoder.default
JSONEncoder.default = wrapped_default

class Context:
    def __init__(self, screen):
        self.uid = ''.join(random.choices(string.ascii_uppercase + string.ascii_lowercase, k=5))
        self.screen = screen

    context_registry = {}
    active_context = None
    active_context_thread = None

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

    def meta_context(self):
        return True

    def thread_uid(self):
        return f"{self.name()}_{self.uid}"

    def id(self):
        return self.name().lower().replace(" ", "_")

    def name(self):
        return "Default Context"
    
    def desc(self):
        return "This is the Default Context"

    def restore(self):
        pass

    def interval_sec(self):
        return 0 

    def to_dict(self):
        return {
                  "id": self.id(),
                  "uid": self.uid,
                  "name": self.name(),
                  "description": self.desc(),
                  "thread_uid": self.thread_uid(),
                  "interval_sec": self.interval_sec(),
                  "meta_context": self.meta_context(),
                }
        
    def __json__(self):
        return self.to_dict()
