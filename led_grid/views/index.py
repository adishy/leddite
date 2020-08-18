from flask import *
import led_grid
import time
import tempfile

@led_grid.app.route('/', methods=['GET'])
def show():
    return render_template('index.html')

@led_grid.app.route('/upload', methods=['POST'])
def upload():
    if request.method == 'POST':
        file = request.files['file']
        _, temp_filename = tempfile.mkstemp()
        file.save(temp_filename)
        print("~~~~~~~", temp_filename)
        led_grid.screen.read_image(temp_filename)
        time.sleep(5) 
        led_grid.screen.clear_grid()       

    #return redirect(url_for('show'))
    return "Helloo"
