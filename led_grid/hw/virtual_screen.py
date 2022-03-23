class VirtualScreen:
    def __init__(self, height, width):
        self.width_v = width
        self.height_v = height
        self.screen = [ [ (0, 0, 0) for _ in range(self.width_v) ] for _ in range(self.height_v) ]
    
    def width(self):
        return self.width_v

    def height(self):
        return self.height_v
        
    def show(self):
        print(chr(27)+'[2j')
        print('\033c')
        print('\x1bc')
        for y in self.screen:
            print(y)
            
    def set_pixel(self, y, x, color, refresh_grid=True):
        self.screen[x][y] = color

    def refresh():
        print("Refreshed!") 

    def __repr__(self):
        screen_diagram = ""
        for i in self.screen:
            screen_diagram += f"{i}\n"
        return screen_diagram

if __name__ == '__main__':
  screen = VirtualScreen(16, 16)
  screen.set_pixel(0, 0, (255, 8, 0))
  screen.show()
