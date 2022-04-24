from flask import *
import led_grid
import time
import tempfile

@led_grid.app.route('/api/v1/', methods=['GET'])
def index():
    """Sets a pixel at a given position and color"""
    return jsonify({
                    "message": "led_grid /api/v1/",
                    "status_code": 200
                   }), 200
