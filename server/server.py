from flask import Flask, request, render_template, send_from_directory, jsonify
import os

app = Flask(__name__)
PICS_FOLDER = "uploads"
os.makedirs(PICS_FOLDER, exist_ok=True)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/upload', methods=['POST'])
def upload_file():
    if 'file' not in request.files:
        return "No file part", 400

    file = request.files['file']
    if file.filename == '':
        return "No selected file", 400

    file_path = os.path.join(PICS_FOLDER, file.filename)
    file.save(file_path)
    return "File uploaded successfully", 200

@app.route('/uploads/<filename>')
def get_uploaded_file(filename):
    return send_from_directory(PICS_FOLDER, filename)

@app.route('/images')
def list_images():
    files = sorted(os.listdir(PICS_FOLDER), key=lambda x: os.path.getmtime(os.path.join(PICS_FOLDER, x)))
    return jsonify(files)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
