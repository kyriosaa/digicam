#include "esp_camera.h"
#include "esp_sleep.h"
#include "SD_MMC.h"
#include "FS.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "env.h"

// wakeup pin config
#define WAKEUP_PIN        13

// camera pin config (ESP32-CAM AI-Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// save image to SD card
void saveImageToSD(camera_fb_t * fb) {
  if (!SD_MMC.begin()) {
    Serial.println("SD Card not available");
    return;
  }
  
  // create filename with timestamp (simple millis)
  String filename = "/photo_" + String(millis()) + ".jpg";
  
  File file = SD_MMC.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  
  file.write(fb->buf, fb->len);
  file.close();
  
  Serial.println("Image saved to SD card: " + filename);
  Serial.println("Image size: " + String(fb->len) + " bytes");
}

// upload image to server from file path
bool uploadFileToServer(String filePath) {
  if (!SD_MMC.begin()) {
    Serial.println("SD Card not available for upload");
    return false;
  }
  
  File file = SD_MMC.open(filePath);
  if (!file) {
    Serial.println("Failed to open file: " + filePath);
    return false;
  }
  
  size_t fileSize = file.size();
  
  HTTPClient http;
  http.begin(SERVER_URL);
  
  // set content type for multipart form data
  String boundary = "----ESP32CAM";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  
  // extract filename from path
  String filename = filePath;
  if (filename.startsWith("/")) {
    filename = filename.substring(1);
  }
  
  // create multipart form data
  String body = "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
  body += "Content-Type: image/jpeg\r\n\r\n";
  
  String footer = "\r\n--" + boundary + "--\r\n";
  
  // calc total length
  int totalLength = body.length() + fileSize + footer.length();
  http.addHeader("Content-Length", String(totalLength));
  
  // send request
  WiFiClient * client = http.getStreamPtr();
  client->print(body);
  
  // read and send file in chunks
  uint8_t buffer[1024];
  while (file.available()) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    client->write(buffer, bytesRead);
  }
  file.close();
  
  client->print(footer);
  
  int httpResponseCode = http.POST("");
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Upload successful for: " + filename);
    Serial.println("Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);
    http.end();
    return true;
  } else {
    Serial.println("Upload failed for: " + filename);
    Serial.println("Error code: " + String(httpResponseCode));
    http.end();
    return false;
  }
}

// upload all images from SD card to server
void uploadAllImages() {
  // check if wifi credentials are properly added in env
  #ifndef SSID
    Serial.println("WiFi credentials not found in env.h");
    return;
  #endif
  
  Serial.println("Connecting to WiFi...");
  WiFi.begin(SSID, PASSWORD);
  
  // wait for connection with timeout
  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    Serial.print(".");
    timeout--;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed");
    return;
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // list all jpg files on SD card
  File root = SD_MMC.open("/");
  if (!root) {
    Serial.println("Failed to open SD card root directory");
    WiFi.disconnect();
    return;
  }
  
  Serial.println("Scanning SD card for images...");
  int totalFiles = 0;
  int uploadedFiles = 0;
  int skippedFiles = 0;
  
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.endsWith(".jpg") || fileName.endsWith(".JPG")) {
      totalFiles++;
      Serial.println("Found image: " + fileName);
      
      String filePath = "/" + fileName;
      bool uploaded = uploadFileToServer(filePath);
      
      if (uploaded) {
        uploadedFiles++;
      } else {
        skippedFiles++;
      }
      
      delay(500); // small delay between uploads
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  
  Serial.println("Upload summary:");
  Serial.println("Total images found: " + String(totalFiles));
  Serial.println("Successfully uploaded: " + String(uploadedFiles));
  Serial.println("Failed/Skipped: " + String(skippedFiles));
  
  WiFi.disconnect();
}

void setup() {
  Serial.begin(115200);

  // detect wakeup cause
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke up from button press!");

    // init SD card
    if (!SD_MMC.begin()) {
      Serial.println("SD Card Mount Failed");
    } else {
      Serial.println("SD Card mounted successfully");
    }

    // init cam
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;

    esp_camera_init(&config);

    // take photo
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    } else {
      Serial.println("Photo taken");
      
      // save img to SD card
      saveImageToSD(fb);
      
      esp_camera_fb_return(fb);
    }

    // deinit cam
    esp_camera_deinit();
    
    // try to upload all images from SD card to server
    Serial.println("Attempting to upload all images to server...");
    uploadAllImages();
  }

  // config wakeup pin (active LOW)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 0);

  Serial.println("Going to sleep...");
  delay(1000); // delay to let serial output flush
  esp_deep_sleep_start();
}

void loop() {
  // wont run
}
