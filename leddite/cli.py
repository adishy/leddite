"""
Usage:    
    leddite serve [--port=<port>] [--screen_type=(virtual|physical)] [--height=<virtual_screen_height>] [--width=<virtual_screen_width>] [--hostname=<hostname>] [--debug]
    leddite context set <context-name> [<context-args>] [--hostname=<hostname>] [--debug]
    leddite context info [--hostname=<hostname>] [--debug]
    leddite carousel (start|stop|info) [--hostname=<hostname>] [--debug]
"""
from docopt import docopt
import requests
import json
import sys
import urllib.parse
import os
import leddite

def api(**kwargs):
    endpoints = {
       "context_set": {
          "url": "/context/set/<context-name>/",
          "params": {
            "<context-name>": ""
          },
          "method": "POST"
       },
       "context_info": {
          "url": "/context/info/all/",
          "method": "GET"
       },
       "carousel_start": {
          "url": "/carousel/start/",
          "method": "POST"
       }, 
       "carousel_stop": {
          "url": "/carousel/stop/",
          "method": "POST"
       },
       "carousel_info": {
          "url": "/carousel/info/",
          "method": "GET"
       }
    }

    hostname = "127.0.0.1:5000"
    if "hostname" in kwargs and len(kwargs["hostname"]) > 0:
        hostname = kwargs["hostname"]
    url_protocol = "http://"
    api_base = "/api/v1"
    endpoint_name = kwargs["endpoint"]
    endpoint_params = {}
    if "params" in endpoints[endpoint_name]:
        endpoint_params = endpoints[endpoint_name]["params"]
        for key in endpoint_params:
            stripped_url_param = key.replace('-', '_').replace('<', '').replace('>', '') 
            if stripped_url_param in kwargs:
                endpoint_params[key] = kwargs[stripped_url_param]
    endpoint_url = endpoints[endpoint_name]["url"]
    for key, value in endpoint_params.items():
        endpoint_url = endpoint_url.replace(key, value)
    url = urllib.parse.urljoin(url_protocol + hostname, api_base + endpoint_url)
    endpoint_method = endpoints[endpoint_name]["method"]
    
    if endpoint_method == "GET":
        response = requests.get(url)
    elif endpoint_method == "POST":
        response = requests.post(url)

    if "debug" in kwargs and kwargs["debug"]:
        try:
            response_text = json.loads(response.text)
        except Exception as e:
            response_text = response.text
        print(json.dumps({
          "endpoint_name": endpoint_name,
          "endpoint_url": endpoint_url,
          "endpoint_method": endpoint_method,
          "request_url_built": url,
          "status": response.status_code,
          "response": response_text
        }, indent=5), file=sys.stderr)

    if response.status_code == 200:
        if "debug" in kwargs and kwargs["debug"]:
            return 0
        print(response.text, file=sys.stderr)
        return 0
    else:
        print(f"Could not request endpoint: {url}", file=sys.stderr)
        print(response.text, file=sys.stderr)
        return 1

def run_cli():
    arguments = docopt(__doc__)

    if arguments['serve']:
        port = 5000
        virtual_screen = arguments["--debug"] or (arguments["--screen_type"] and arguments["--screen_type"] == "virtual")
        virtual_screen_height = 16
        virtual_screen_width = 16
        if arguments['--port']:
            port = int(arguments['--port'])
        leddite.run(port, virtual_screen, virtual_screen_height, virtual_screen_width)
   
    hostname = arguments['--hostname'] 
    if not hostname:
        hostname = "127.0.0.1:5000"

    debug = False 
    if arguments['--debug']:
        debug = True 

    if arguments['context']:
        if arguments['info']:
            exit(api(endpoint="context_info", hostname=hostname, debug=debug))
        elif arguments['set']:
            context_name = arguments['<context-name>']
            exit(api(endpoint="context_set", context_name=context_name, hostname=hostname, debug=debug))
    
    if arguments['carousel']: 
        if arguments['start']:
            exit(api(endpoint="carousel_start", hostname=hostname, debug=debug))
        elif arguments['stop']:
            exit(api(endpoint="carousel_stop", hostname=hostname, debug=debug))
        else:
            exit(api(endpoint="carousel_info", hostname=hostname, debug=debug))


if __name__ == "__main__":
    run_cli()
