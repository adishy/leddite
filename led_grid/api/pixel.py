from flask import *
import led_grid
import time
import tempfile

@led_grid.app.route('/api/v1/pixel/set', methods=['POST'])
def pixel_set():
    """Sets a pixel at a given position and color"""
    request_data = json.loads(request.data)
    
    try:
        x = int(request_data['position']['x'])
        y = int(request_data['position']['y'])
        r = int(request_data['color']['r'])
        g = int(request_data['color']['g'])
        b = int(request_data['color']['b'])

        led_grid.screen.set_pixel_rgb(x, y, r, g, b)
        
        return jsonify({
                        "message": "Finished setting pixel!",
                        "position": { "x": x, "y": y },
                        "color": { "r": r, "g": g, "b": b },
                        "status_code": 200
                       }), 200
   
    except Exception as e:
        return jsonify({
                        "message": "Unexpected error deleting pixel",
                        "error": e,
                        "status_code": 500
                       }), 500


@led_grid.app.route('/api/v1/pixel/delete', methods=['POST'])
def pixel_delete():
   """Deletes a pixel at a given position"""
   request_data = json.loads(request.data)
   
   try:
       x = int(request_data['position']['x'])
       y = int(request_data['position']['y'])
       
       # "Erasing" a pixel just sets it to black
       led_grid.screen.set_pixel_rgb(x, y, 0, 0, 0)
       
       return jsonify({
                       "message": "Finished deleting pixel!",
                       "position": { "x": x, "y": y },
                       "status_code": 200
                      }), 200

   except Exception as e:
       return jsonify({
                       "message": "Unexpected error deleting pixel",
                       "error": e,
                       "status_code": 500
                      }), 500


@led_grid.app.route('/api/v1/pixel/set_and_delete_multiple', methods=['POST'])
def pixel_set_and_delete_multiple():
    """Sets an array of pixels at specified positions and colors"""
    request_data = json.loads(request.data)
   
    print("pixels:", request_data)

    for pixel in request_data['pixels']:
        try:
            action = pixel['action']
            
            x = int(pixel['position']['x'])
            y = int(pixel['position']['y'])

            if action == "set":
                r = int(pixel['color']['r'])
                g = int(pixel['color']['g'])
                b = int(pixel['color']['b'])

                led_grid.screen.set_pixel_rgb(x, y, r, g, b, False)
            else:
                led_grid.screen.set_pixel_rgb(x, y, 0, 0, 0, False)
        except Exception as e:
            return jsonify({
                            "message": "Unexpected error setting and erasing pixels",
                            "error": e,
                            "status_code": 500
                           }), 500
    
    led_grid.screen.refresh()
    
    return jsonify({
                    "message": "Finished setting pixels!",
                    "number_changed": len(request_data['pixels']),
                    "status_code": 200
                   }), 200


@led_grid.app.route('/api/v1/pixel/delete_all', methods=['POST'])
def pixel_delete_all():
   """Deletes all pixels"""
   led_grid.screen.clear_grid()

   return jsonify({ 
                    "message": "Erased content of screen",
                    "status_code": 200
                  }), 200

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

