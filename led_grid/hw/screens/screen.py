from threading import Lock
import sys
import threading

class Screen:
   def __init__(self):
     pass 

   switcher_lock = Lock()
   thread_uid = None
 
   def height(self):
      return -1

   def width(self):
      return -1

   def color(self, color_tuple):
      return color_tuple

   def context(self):
      return self.active_context

   def permission(self):
      check_permission = threading.current_thread().name == Screen.thread_id
      if not check_permission:
        print(f"I tried to write to the screen with no permission: Me: {threading.current_thread().name}. Current: {Screen.thread_uid} Bad Dobby!", file=sys.stderr)
      return check_permission

   def set_pixel(self, x, y, color, refresh_grid = True):
      pass

   def clear(self):
        pass

   def refresh(self):
        pass
