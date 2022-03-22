class VirtualScreen:
    def __init__(self, height, width):
        self.width_v = width
        self.height_v = height
        self.default_color = (0, 0, 0)
        row = [ self.default_color for _ in range(self.width) ]
        self.screen = [ row for _ in range(self.height) ]
    
    def width(self):
        return self.width_v

    def height(self):
        return self.height_v
        
    def show(self):
        for i in screen:
            print(i)
            
    def set_pixel(x, y, color):
        self.screen[x][y] = color

    def __repr__(self):
        screen_diagram = ""
        for i in self.screen:
            screen_diagram += f"{i}\n"
        return screen_diagram

if __name__ == '__main__':
    screen = VirtualScreen(16, 16)
    screen.set_pixel(0, 0, (255, 1, 0))
    screen.show()
