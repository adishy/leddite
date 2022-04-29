from led_grid.hw.screens.screen import Screen
from threading import Thread, Lock, Event
import random
import string
class Context:
    def __init__(self, screen):
        self.uid = ''.join(random.choices(string.ascii_uppercase + string.ascii_lowercase, k=5))
        self.screen = screen
    
    #context_switch_lock = Lock()
    active_context = None
    active_context_thread = None
    #context_changed = Event()
    #running_context_done = Event()

    @classmethod
    def set_context(cls, new_context):
      #if cls.active_context_thread:
      #    cls.__switch(new_context) 
      cls.__set_new_context(new_context)

    @classmethod
    def __set_new_context(cls, new_context):
       Screen.switcher_lock.acquire()
       Screen.thread_id = new_context.thread_uid()
       cls.active_context = new_context
       cls.active_context_thread = Thread(target=cls.active_context.show, name=new_context.thread_uid())
       cls.active_context_thread.start()
       Screen.switcher_lock.release()
    
    #@classmethod
    #def __switch(cls, new_context):
    #   #cls.context_changed.set()
    #   #cls.running_context_done.wait()
    #   print("Previous thread alive:", cls.active_context_thread.is_alive())
    #   print("in switching context")
    #   Screen.screen_lock.release()
    #   #cls.running_context_done.clear()
    #   #cls.context_changed.clear()
    #   cls.__set_new_context(new_context)

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

    def switch(self, next_context):
        pass
