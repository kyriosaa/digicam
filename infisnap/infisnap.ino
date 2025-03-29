#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <HTTPClient.h>
#include <FS.h>
#include <SD_MMC.h>
#include "env.h"

const char* ssid = SSID;
const char* password = PASSWORD;
const char* serverURL = SERVER_URL;

boolean takeNewPhoto = false;
#define FILE_PHOTO "/photo.jpg"
#define BUTTON_PIN 13  

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void IRAM_ATTR buttonISR() {
    takeNewPhoto = true;
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi");
    } else {
        Serial.println("WiFi connected");
    }

    if (!SD_MMC.begin()) {
        Serial.println("SD Card Mount Failed");
        return;
    }

    pinMode(BUTTON_PIN, INPUT_PULLUP);  // Configure button pin
    attachInterrupt(BUTTON_PIN, buttonISR, FALLING);  // Attach interrupt
}

void loop() {
    if (takeNewPhoto) {
        capturePhotoSaveSD();
        takeNewPhoto = false;

        if (WiFi.status() == WL_CONNECTED) {
            uploadImages();
        }
    }
    delay(1);
}

void capturePhotoSaveSD() {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }
    
    File file = SD_MMC.open(FILE_PHOTO, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
    } else {
        file.write(fb->buf, fb->len);
        Serial.println("Photo saved to SD card");
    }
    file.close();
    esp_camera_fb_return(fb);
}

void uploadImages() {
    File file = SD_MMC.open(FILE_PHOTO);
    if (!file) {
        Serial.println("No image to upload");
        return;
    }

    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "image/jpeg");

    int fileSize = file.size();
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
        Serial.println("Memory allocation failed");
        return;
    }

    file.read(buffer, fileSize);
    int httpResponseCode = http.POST(buffer, fileSize);
    free(buffer);
    file.close();
    http.end();

    if (httpResponseCode == 200) {
        Serial.println("Upload successful, deleting file.");
        SD_MMC.remove(FILE_PHOTO);
    } else {
        Serial.println("Upload failed");
    }
}
