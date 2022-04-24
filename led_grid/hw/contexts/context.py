from threading import Thread, Event

class Context:
    def __init__(self, screen):
        self.screen = screen
        self.switch_ready = False
        self.switch_context = False
        self.switch_ready = False

    prev_context = None
    active_context = None
    active_context_thread = None

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
       cls.active_context.signal_switch()
       while not cls.active_context.ready_to_switch():
           pass 
       cls.__set_new_context(new_context)
  
    def show(self):
        pass

    def signal_switch(self):
        self.switch_context = True

    def ready_to_switch(self):
        return self.switch_ready

    def desc(self):
        return "Context"

    def restore(self):
        pass

    def switch(self, next_context):
        pass
