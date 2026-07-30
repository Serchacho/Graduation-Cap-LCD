#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// AP credentials
const char* ssid = "GraduationPhotoBooth";
const char* password = "celebrate2026";

IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);

// PSRAM buffer for the incoming image
uint8_t* imageBuffer = nullptr;
size_t imageBufferSize = 0;
size_t imageBytesReceived = 0;
const size_t MAX_IMAGE_SIZE = 500 * 1024; // 500KB cap - adjust based on your resize target

volatile bool newImageReady = false;

// Simple HTML upload page with client-side canvas resize before POST
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Send a Photo!</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; padding: 20px; background: #222; color: #fff; }
    input[type=file] { margin: 20px 0; }
    button { padding: 12px 24px; font-size: 18px; }
    #status { margin-top: 15px; }
  </style>
</head>
<body>
  <h2>Send Your Photo to the Cap!</h2>
  <input type="file" id="fileInput" accept="image/*" capture="environment"><br>
  <button onclick="uploadImage()">Send</button>
  <div id="status"></div>

  <script>
    const TARGET_W = 320; // match your LCD resolution
    const TARGET_H = 240;

    function uploadImage() {
      const fileInput = document.getElementById('fileInput');
      const file = fileInput.files[0];
      if (!file) {
        document.getElementById('status').innerText = "Pick a photo first!";
        return;
      }

      const img = new Image();
      const reader = new FileReader();

      reader.onload = function(e) {
        img.onload = function() {
          const canvas = document.createElement('canvas');
          canvas.width = TARGET_W;
          canvas.height = TARGET_H;
          const ctx = canvas.getContext('2d');

          // Letterbox: scale to fit while preserving aspect ratio
          const scale = Math.min(TARGET_W / img.width, TARGET_H / img.height);
          const drawW = img.width * scale;
          const drawH = img.height * scale;
          const offsetX = (TARGET_W - drawW) / 2;
          const offsetY = (TARGET_H - drawH) / 2;

          ctx.fillStyle = 'black';
          ctx.fillRect(0, 0, TARGET_W, TARGET_H);
          ctx.drawImage(img, offsetX, offsetY, drawW, drawH);

          canvas.toBlob(function(blob) {
            const formData = new FormData();
            formData.append('image', blob, 'photo.jpg');

            document.getElementById('status').innerText = "Sending...";

            fetch('/upload', {
              method: 'POST',
              body: formData
            })
            .then(response => response.text())
            .then(result => {
              document.getElementById('status').innerText = "Sent! Look at the cap!";
            })
            .catch(error => {
              document.getElementById('status').innerText = "Error sending photo.";
            });
          }, 'image/jpeg', 0.7); // 0.7 quality - good balance for this use case
        };
        img.src = e.target.result;
      };
      reader.readAsDataURL(file);
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Allocate the image buffer in PSRAM
  imageBuffer = (uint8_t*)ps_malloc(MAX_IMAGE_SIZE);
  if (imageBuffer == nullptr) {
    Serial.println("Failed to allocate PSRAM buffer!");
  } else {
    Serial.printf("PSRAM buffer allocated: %d bytes\n", MAX_IMAGE_SIZE);
  }

  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password, 1, 0, 8); // channel 1, not hidden, max 8 connections

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Serve the upload page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Handle the image upload
  server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      // This runs after the upload completes
      request->send(200, "text/plain", "OK");
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      // This is the upload handler - called repeatedly as data streams in

      if (index == 0) {
        // First chunk - reset our tracking
        imageBytesReceived = 0;
        Serial.printf("Upload started: %s\n", filename.c_str());
      }

      // Bounds check before copying
      if (imageBytesReceived + len <= MAX_IMAGE_SIZE) {
        memcpy(imageBuffer + imageBytesReceived, data, len);
        imageBytesReceived += len;
      } else {
        Serial.println("Image too large - truncating");
      }

      if (final) {
        imageBufferSize = imageBytesReceived;
        Serial.printf("Upload complete: %d bytes total\n", imageBufferSize);
        newImageReady = true;
      }
    }
  );

  server.begin();
  Serial.println("Server started");
}

void loop() {
  if (newImageReady) {
    newImageReady = false;
    Serial.println("New image ready for decode/display step");
    // Next step: decode imageBuffer (imageBufferSize bytes) and push to LCD
  }
}