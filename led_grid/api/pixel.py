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


