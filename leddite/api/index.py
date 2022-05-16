from flask import *
import leddite
import urllib.parse
import time
import tempfile

@leddite.app.route('/api/v1/', methods=['GET'])
def index():
    def list_routes():
        output = []
        for rule in leddite.app.url_map.iter_rules():
            options = {}
            for arg in rule.arguments:
                options[arg] = "[{0}]".format(arg)
            methods = ','.join(rule.methods)
            url = url_for(rule.endpoint, **options)
            output.append(f"{urllib.parse.unquote(url)} {methods.replace(',OPTIONS', '').replace('OPTIONS,', '')}")
        return output
   
 
        return jsonify({
                    "message": "leddite /api/v1/",
                    "endpoints": list_routes(),
                    "status_code": 200
                   }), 200
