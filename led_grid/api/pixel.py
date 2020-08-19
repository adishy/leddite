from flask import *
import led_grid
import time
import tempfile

@led_grid.app.route('/api/v1/pixel/set', methods=['POST'])
def pixel_set():
    """Sets a pixel at a given position and color"""
    if request.method == 'POST':
        request_data = json.loads(request.data)
        
        expected_payload_keys = ['position', 'color']
        position_keys = ['x', 'y']
        color_keys = ['r', 'g', 'b']

        if not all(key in request_data for key in expected_keys):
            return jsonify({
                            "message": "No JSON arguments!",
                            "status_code": 400
                           }), 400

        if not all(key in request_data['position'] for key in position_keys):
            return jsonify({
                            "message": "No position arguments!",
                            "status_code": 400
                           }), 400
        if not all(key in request_data['color'] for key in color_keys):
            return jsonify({
                            "message": "No color arguments!",
                            "status_code": 400
                           }), 400
       
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
                            "color": { "r": r, "g": g, "b", b },
                            "status_code": 200
                           }), 200

        except:
            return jsonify({
                            "message": "Unexpected error setting pixel",
                            "status_code": 500
                           }), 500

@led_grid.app.route('/api/v1/pixel/delete', methods=['POST'])
def pixel_delete():
   """Deletes a pixel at a given position"""
   if request.method == 'POST':
        request_data = json.loads(request.data)
        
        expected_payload_keys = ['position', 'color']
        position_keys = ['x', 'y']

        if not all(key in request_data for key in expected_keys):
            return jsonify({
                            "message": "No JSON arguments!",
                            "status_code": 400
                           }), 400

        if not all(key in request_data['position'] for key in position_keys):
            return jsonify({
                            "message": "No position arguments!",
                            "status_code": 400
                           }), 400
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

        except:
            return jsonify({
                            "message": "Unexpected error deleting pixel",
                            "status_code": 500
                           }), 500


@led_grid.app.route('/api/v1/pixel/delete_all', methods=['POST'])
def pixel_delete_all():
   """Deletes all pixels"""
    if request.method == 'POST':
        led_grid.screen.clear_grid()

    return jsonify({ 
                     "message": "Erased content of screen",
                     "status_code": 200
                   }), 200


