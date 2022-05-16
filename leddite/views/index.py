from flask import *
import leddite
import time
import tempfile

@leddite.app.route('/', methods=['GET'])
def show():
    return render_template('index.html')

@leddite.app.route('/editor', methods=['GET'])
def editor():
    return render_template('pixel_editor.html')

@leddite.app.route('/upload', methods=['POST'])
def upload():
    if request.method == 'POST':
        file = request.files['file']
        _, temp_filename = tempfile.mkstemp()
        file.save(temp_filename)
        leddite.screen.read_image(temp_filename)

    return redirect(url_for('show'))

@leddite.app.route('/erase_screen', methods=['POST'])
def erase_screen():
    if request.method == 'POST':
        leddite.screen.clear_grid()

    return redirect(url_for('show'))

