class Screen:
   def __init__(self):
      pass

   def height(self):
      return -1

   def width(self):
        return -1

   def color(self, color_tuple):
        return f"transformed {color_tuple}"

   def set_pixel(self, x, y, color, refresh_grid = True):
        pass

   def clear(self):
        pass

   def refresh(self):
        pass
