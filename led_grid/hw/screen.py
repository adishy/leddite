import imageio
from rpi_ws281x import *
import time

class Screen:
   def __init__(self):
      # LED strip configuration:
      self.LED_ROWS       = 16
      self.LED_COLUMNS    = 16
      self.LED_COUNT      = self.LED_ROWS * self.LED_COLUMNS # Number of LED pixels.
      self.LED_PIN        = 18      # GPIO pin connected to the pixels (18 uses PWM!).
      self.LED_FREQ_HZ    = 800000  # LED signal frequency in hertz (usually 800khz)
      self.LED_DMA        = 10      # DMA channel to use for generating signal (try 10)
      self.LED_BRIGHTNESS = 10     # Set to 0 for darkest and 255 for brightest
      self.LED_INVERT     = False   # True to invert the signal (when using NPN transistor level shift)
      self.LED_CHANNEL    = 0       # set to '1' for GPIOs 13, 19, 41, 45 or 53

      row = [ Color(0, 0, 0) for x in range(self.LED_COLUMNS) ]
      self.current_grid = [ row for y in range(self.LED_ROWS) ]

      self.strip = Adafruit_NeoPixel(self.LED_COUNT,
                                     self.LED_PIN,
                                     self.LED_FREQ_HZ,
                                     self.LED_DMA,
                                     self.LED_INVERT,
                                     self.LED_BRIGHTNESS,
                                     self.LED_CHANNEL)
      
      # Intialize the library (must be called once before other functions).
      self.strip.begin()

   def height(self):
      return self.LED_ROWS

   def width(self):
      return self.LED_COLUMNS

   def set_pixel(self, x, y, color, refresh_grid = True):
      if x < 0 or x > self.LED_ROWS - 1:
        raise "Invalid row"
      if y < 0 or y > self.LED_COLUMNS - 1:
        raise "Invalid column"
      # Normalizing y co-ordinate as in the LED grid, the order
      # flip-flops
      if (x % 2) > 0:
         y = self.LED_ROWS - 1 - y   
      position_in_grid = x * self.LED_ROWS + y
      self.strip.setPixelColor(position_in_grid, color)
      if refresh_grid:
         self.strip.show()

   def set_pixel_rgb(self, x, y, r, g, b, refresh_grid = True):
      self.set_pixel(x, y, Color(r, g, b), refresh_grid)

   def set_image(self, image):
      #rows, columns, channels = image.shape
      #print("Rows:", rows, "Columns:", columns, "Channels:", channels)
      #print("Actual rows:", self.LED_ROWS, "Actual columns:", self.LED_COLUMNS)
      #assert rows > 0 and rows <= self.LED_ROWS
      #assert columns > 0 and columns <= self.LED_COLUMNS
      #assert channels >= 3
      for i, row in enumerate(self.current_grid):
        for j, current_color_value in enumerate(row):
             new_color_value = Color(int(image[i][j][0]), int(image[i][j][1]), int(image[i][j][2]))
             if current_color_value != new_color_value:
                 self.current_grid[i][j] = new_color_value 
                 self.set_pixel(i, j, new_color_value, False)
      self.strip.show()

   def overwrite_image(self, image):
      for i, row in enumerate(image):
         for j, value in enumerate(row):
             new_color_value = Color(int(image[i][j][0]), int(image[i][j][1]), int(image[i][j][2]))
             self.set_pixel(i, j, new_color_value, False)
      self.strip.show()
   
   def read_image(self, path):
      image = imageio.imread(path)
      self.overwrite_image(image)

   def is_virtual(self):
      return False
 
   def clear_grid(self):
      blank_image = [ [ (0, 0, 0) for value in range(self.LED_COLUMNS) ] for row in range(self.LED_ROWS) ] 
      self.overwrite_image(blank_image)

   def refresh(self):
      self.strip.show()

if __name__ == "__main__":
    grid = Screen()
    white = Color(255, 255, 255)
    #for i in range(16):
    #   if i % 2 > 0:
    #      grid.set_pixel(i, 15, white) 
    #   else:
    #      grid.set_pixel(i, 0, white)
    path = "/home/pi/test.png"
    grid.read_image(path)    
    #image = [ [ ( 255, 255, 0) for j in range(16) ] for i in range(16) ] 
    #grid.overwrite_image(image) 
    time.sleep(5) 
    grid.clear_grid()       
