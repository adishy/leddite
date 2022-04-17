from led_grid.hw.contexts import Context

class Blank(Context):
    def __init__(self, color=(0,0,0)):
        super().__init__()
        self.background_color = color

    def show(self, screen):
       for i in range(screen.width()):
            for j in range(screen.height()):
                screen.set_pixel(i, j, screen.color(self.color))
 
    def desc(self):
        return "Blank Context: background color: rgb({self.background_color})" 
        
