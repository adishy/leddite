class Screen:
   def __init__(self):
      pass
      
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

   def refresh(self):
        pass
        #self.state_change = False
