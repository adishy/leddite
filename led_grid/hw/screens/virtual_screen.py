from rich import print as rprint
from rich.console import Console
from led_grid.hw.screens.screen import Screen
import datetime

class VirtualScreen(Screen):
    def __init__(self, height, width):
        self.width_v = width
        self.height_v = height
        self.screen = [ [ "rgb(0,0,0)" for _ in range(self.width_v) ] for _ in range(self.height_v) ]
        self.last_refresh = "NA"
        self.console = Console()
    
    
    def width(self):
        return self.width_v

    def height(self):
        return self.height_v

    def color(self, color_tuple):
        return f"rgb({color_tuple[0]},{color_tuple[1]},{color_tuple[2]})"

    def show(self, debug=False):
        self.console.clear()
        if debug:
           for y in self.screen:
               print(y)
        else:
           for y, row in enumerate(self.screen):
               for x, color in enumerate(row):
                   if x == len(row) - 1:
                       end_row = "\n"
                   else:
                       end_row = ""
                   self.console.print("◘",
                                      style=color,
                                      end=end_row)
           self.console.print(f"Screen dimensions: {self.height_v} x {self.height_v}, Refreshed: {self.last_refresh}")

    def set_pixel(self, y, x, color, refresh_grid=True):
        self.screen[x][y] = color

    def refresh(self, debug=False):
        self.last_refresh = datetime.datetime.now()
        self.show(debug)

    def __repr__(self):
        screen_diagram = ""
        for i in self.screen:
            screen_diagram += f"{i}\n"
        return screen_diagram

if __name__ == '__main__':
  screen = VirtualScreen(16, 16)
  screen.set_pixel(0, 0, (255, 8, 0))
  screen.show()
