from threading import Thread, Event

class Context:
    def __init__(self, screen):
        self.screen = screen

    prev_context = None
    active_context = None
    active_context_thread = None
    context_changed = Event()
    running_context_done = Event()

    @classmethod
    def set_context(cls, new_context):
      cls.prev_context = cls.active_context
      if cls.active_context_thread:
          cls.__switch(new_context) 
      cls.__set_new_context(new_context)

    @classmethod
    def __set_new_context(cls, new_context): 
       cls.active_context = new_context
       cls.active_context_thread = Thread(target=cls.active_context.show)
       cls.active_context_thread.start()
    
    @classmethod
    def __switch(cls, new_context):
       cls.context_changed.set()
       cls.running_context_done.wait()
       print("in switching context")
       cls.running_context_done.clear()
       cls.context_changed.clear()
       cls.__set_new_context(new_context)

    @classmethod
    def __indicate(cls):
        cls.need_to_switch = True

    def show(self):
        pass

    def name(self):
        return "Default Context"
    
    def desc(self):
        return "This is the Default Context"

    def restore(self):
        pass

    def switch(self, next_context):
        pass
