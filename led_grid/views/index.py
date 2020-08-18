from flask import *
import led_grid

@led_grid.app.route('/', methods=['GET'])
def show():
    return render_template('index.html')

@led_grid.app.route('/upload', methods=['POST'])
def upload():
    if request.method == 'POST' and not request.files.get('file', None):
        file = request.files['file']
        _, temp_filename = tempfile.mkstemp()
        file.save(temp_filename)
        led_grid.screen.read_image(temp_filename)

    return redirect(url_for('show'))
