from led_grid.hw.contexts import Context

class Blank(Context):
    def __init__(self,screen, color=(0,0,0)):
        super().__init__(screen)
        self.background_color = color

    def show(self):
       self.switch_ready = False
       for i in range(self.screen.width()):
            for j in range(self.screen.height()):
                self.screen.set_pixel(i, j, self.screen.color(self.background_color))
       self.switch_context = False
       self.switch_ready = True
 
    def desc(self):
        return f"Blank Context: background color: rgb({self.background_color})" 
        
