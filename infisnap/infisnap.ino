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
    return;
  }
  
  // basically will generate a random file name for the image
  uint32_t bootCount = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED ? 
                       esp_random() : millis();
  uint32_t randomNum = esp_random() % 10000;
  
  String filename = "/photo_" + String(bootCount) + "_" + String(randomNum) + ".jpg";
  
  File file = SD_MMC.open(filename, FILE_WRITE);
  if (!file) {
    return;
  }
  
  file.write(fb->buf, fb->len);
  file.close();
}

// upload image to server from file path
bool uploadFileToServer(String filePath) {
  Serial.println("Uploading: " + filePath);
  
  if (!SD_MMC.begin()) {
    Serial.println("SD failed in upload");
    return false;
  }
  
  File file = SD_MMC.open(filePath);
  if (!file) {
    Serial.println("File open failed: " + filePath);
    return false;
  }
  
  size_t fileSize = file.size();
  Serial.println("File size: " + String(fileSize));
  
  if (fileSize > 50000) { // skip any files larger than 50KB bcs it will crash
    Serial.println("File too large, skipping");
    file.close();
    return false;
  }
  
  // read file into buffer
  uint8_t* fileBuffer = (uint8_t*)malloc(fileSize);
  if (!fileBuffer) {
    Serial.println("Memory allocation failed");
    file.close();
    return false;
  }
  
  file.read(fileBuffer, fileSize);
  file.close();
  Serial.println("File read complete");
  
  HTTPClient http;
  Serial.println("Starting HTTP request to: " + String(SERVER_URL));
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/octet-stream");
  
  // get file name from path
  String filename = filePath;
  if (filename.startsWith("/")) {
    filename = filename.substring(1);
  }
  http.addHeader("X-Filename", filename);
  Serial.println("Headers set, filename: " + filename);
  
  // send POST request with raw file data
  Serial.println("Sending POST request...");
  int httpResponseCode = http.POST(fileBuffer, fileSize);
  Serial.println("Response code: " + String(httpResponseCode));
  
  free(fileBuffer);
  http.end();
  
  return (httpResponseCode == 200);
}

// upload all images from SD card to server
void uploadAllImages() {
  // check if wifi credentials are properly added in env
  #ifndef SSID
    Serial.println("No WiFi credentials");
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
    Serial.println("\nWiFi failed");
    return;
  }
  
  Serial.println("\nWiFi connected!");
  
  // list all jpg files on SD card
  File root = SD_MMC.open("/");
  if (!root) {
    Serial.println("SD root failed");
    WiFi.disconnect();
    return;
  }
  
  Serial.println("Scanning for images...");
  int totalFiles = 0;
  int uploadedFiles = 0;
  int skippedFiles = 0;
  
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.endsWith(".jpg") || fileName.endsWith(".JPG")) {
      totalFiles++;
      Serial.println("Found: " + fileName);
      
      String filePath = "/" + fileName;
      bool uploaded = uploadFileToServer(filePath);
      
      if (uploaded) {
        uploadedFiles++;
        Serial.println("Uploaded: " + fileName);
      } else {
        skippedFiles++;
        Serial.println("Failed: " + fileName);
      }
      
      delay(500); // small delay between uploads
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  
  Serial.println("Upload summary: " + String(uploadedFiles) + "/" + String(totalFiles));
  WiFi.disconnect();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 Starting...");

  // detect wakeup cause
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.println("Wakeup reason: " + String(wakeup_reason));
  
  // On first boot or button press, take photo and upload
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("Taking photo and uploading...");

    // init SD card
    if (!SD_MMC.begin()) {
      Serial.println("SD Card failed");
    } else {
      Serial.println("SD Card OK");
    }

    // init cam
    Serial.println("Initializing camera...");
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
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.println("Camera init failed");
    } else {
      Serial.println("Camera OK");
      
      sensor_t * s = esp_camera_sensor_get();
      if (s) {
        // img white balance
        s->set_whitebal(s, 1);       
        s->set_awb_gain(s, 1);       
        
        // img exposure
        s->set_exposure_ctrl(s, 1); 
        s->set_aec2(s, 0);           
        s->set_ae_level(s, 0);       
        
        // img gain
        s->set_gain_ctrl(s, 1);     
        s->set_agc_gain(s, 0);       
        
        // img adjustments
        s->set_brightness(s, 1);    
        s->set_contrast(s, 0);       
        s->set_saturation(s, -2);    
        
        // sfx
        s->set_special_effect(s, 0); 
        
        // color bar test pattern
        s->set_colorbar(s, 0);
      }
    }

    // take photo
    Serial.println("Taking photo...");
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Photo failed");
    } else {
      Serial.println("Photo taken, size: " + String(fb->len));
      
      // save img to SD card
      saveImageToSD(fb);
      Serial.println("Photo saved to SD");
      
      esp_camera_fb_return(fb);
    }

    // deinit cam
    esp_camera_deinit();
    Serial.println("Camera deinitialized");
    
    // delay and free memory before upload
    delay(2000);
    Serial.println("Free heap before upload: " + String(ESP.getFreeHeap()));
    
    // try to upload all images from SD card to server
    Serial.println("Starting upload...");
    uploadAllImages();
    Serial.println("Upload complete");
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
