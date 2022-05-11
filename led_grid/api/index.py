from flask import *
import led_grid
import urllib.parse
import time
import tempfile

@led_grid.app.route('/api/v1/', methods=['GET'])
def index():
    def list_routes():
        output = []
        for rule in led_grid.app.url_map.iter_rules():
            options = {}
            for arg in rule.arguments:
                options[arg] = "[{0}]".format(arg)
            methods = ','.join(rule.methods)
            url = url_for(rule.endpoint, **options)
            output.append(f"{urllib.parse.unquote(url)} {methods.replace(',OPTIONS', '').replace('OPTIONS,', '')}")
        return output
   
 
        return jsonify({
                    "message": "led_grid /api/v1/",
                    "endpoints": list_routes(),
                    "status_code": 200
                   }), 200
