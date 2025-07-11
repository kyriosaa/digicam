from flask import Flask, request, render_template, send_from_directory, jsonify
import os
import time

app = Flask(__name__)
PICS_FOLDER = "uploads"
os.makedirs(PICS_FOLDER, exist_ok = True)

@app.before_request
def log_request_info():
    print(f"Request: {request.method} {request.path}")
    if request.method == 'POST':
        print(f"Content-Type: {request.content_type}")
        print(f"Content-Length: {request.content_length}")

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/health')
def health_check():
    return "Server is running", 200

@app.route('/upload', methods = ['POST'])
def upload_file():
    print("=== UPLOAD REQUEST RECEIVED ===")
    print(f"Upload request received. Content-Type: {request.content_type}")
    print(f"Content-Length: {request.content_length}")
    
    # Handle raw binary upload
    if request.content_type == 'application/octet-stream':
        filename = request.headers.get('X-Filename', f'photo_{int(time.time())}.jpg')
        print(f"Filename from header: {filename}")
        
        file_path = os.path.join(PICS_FOLDER, filename)
        print(f"Saving to: {file_path}")
        
        # Check if file already exists
        if os.path.exists(file_path):
            print(f"File already exists: {file_path}")
            return f"File '{filename}' already exists, skipping upload", 409
        
        try:
            # Get raw data from request
            file_data = request.get_data()
            with open(file_path, 'wb') as f:
                f.write(file_data)
            
            file_size = os.path.getsize(file_path)
            print(f"File saved successfully. Size: {file_size} bytes")
            return f"File '{filename}' uploaded successfully", 200
        except Exception as e:
            print(f"Error saving file: {e}")
            return f"Error saving file: {e}", 500
    
    # Handle multipart form upload (existing code)
    if 'file' not in request.files:
        print("No 'file' in request.files")
        return "No file part", 400

    file = request.files['file']
    print(f"File object: {file}")
    print(f"Filename: {file.filename}")
    
    if file.filename == '':
        print("Empty filename")
        return "No selected file", 400

    file_path = os.path.join(PICS_FOLDER, file.filename)
    print(f"Saving to: {file_path}")
    
    # Check if file already exists
    if os.path.exists(file_path):
        print(f"File already exists: {file_path}")
        return f"File '{file.filename}' already exists, skipping upload", 409
    
    try:
        file.save(file_path)
        file_size = os.path.getsize(file_path)
        print(f"File saved successfully. Size: {file_size} bytes")
        return f"File '{file.filename}' uploaded successfully", 200
    except Exception as e:
        print(f"Error saving file: {e}")
        return f"Error saving file: {e}", 500

@app.route('/uploads/<filename>')
def get_uploaded_file(filename):
    return send_from_directory(PICS_FOLDER, filename)

@app.route('/images')
def list_images():
    files = sorted(os.listdir(PICS_FOLDER), key = lambda x: os.path.getmtime(os.path.join(PICS_FOLDER, x)))
    # Filter only image files
    image_files = [f for f in files if f.lower().endswith(('.jpg', '.jpeg', '.png', '.gif'))]
    return jsonify(image_files)

if __name__ == '__main__':
    app.run(host = '0.0.0.0', port = 5000)
