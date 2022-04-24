from threading import Condition, Lock

class Screen:
   def __init__(self):
      lock = Lock()
      self.lock = lock
      self.cv = Condition(lock=lock)
      self.stage_change = False
      
   def height(self):
      return -1

   def width(self):
      return -1

   def color(self, color_tuple):
      return color_tuple

   def context(self):
      return self.active_context

   def set_pixel(self, x, y, color, refresh_grid = True):
        pass

   def clear(self):
        pass

   def changed(self):
        return self.state_change

   def mark_changed(self):
        with self.cv:
            self.state_change = True
            self.cv.notify()
 
   def refresh(self):
        self.state_change = False
