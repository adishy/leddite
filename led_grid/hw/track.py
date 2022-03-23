class Track:
    def __init__(self,
                 screen,
                 track_height,
                 track_width,
                 horizontal_shift,
                 vertical_shift,
                 shift_timing_ms=300):
        self.screen = screen
        self.timing = shift_timing_ms
        self.height = track_height
        self.width = track_width
        self.horizontal_shift = horizontal_shift
        self.vertical_shift = vertical_shift
        self.current_horizontal_shift = 0
        
        # List of color values, to be displayed, col major
        # * This represents the "actual" track that might 
        # extend off the edge of the screen. 
        # * When the content is rendered, it will be displayed
        # self.width columns at a time until all the columns
        # in the content are shown (at which point it will 
        # cycle back to the beginning
        self.contents = [ ]
        
    def add_content(self, content):
        self.contents += content
    
    def get_contents(self, x, y):
        return self.contents[ ( x * self.height ) + y ]
    
    def get_contents_width(self):
        return int(len(self.contents) / self.height)
    
    def write_to_screen(self):
        for x in range(self.width):
            for y in range(self.height):
                contents_x_val = ( x + self.current_horizontal_shift ) % self.get_contents_width()
                self.screen.set_pixel(x + self.horizontal_shift,
                                      y + self.vertical_shift,
                                      self.get_contents(contents_x_val, y),
                                      False)
        self.screen.refresh()


    def horizontal_shift_one(self):
        self.current_horizontal_shift = ( self.current_horizontal_shift + 1 ) % self.get_contents_width()


