/*
===============================================
📘 README - ESP32 RS485 LED Slave Firmware
===============================================

🔧 FEATURES:
- Listens for RS485 text or binary commands from master.
- Controls 900 NeoPixels (36 boxes of 25 pixels each).
- Web-based Serial Console via WebSocket.
- OTA firmware updates via Web browser.
- Auto hostname: slave<ID>.local (e.g., slave3.local).
- Stores slave ID in internal flash (Preferences).

🟢 SUPPORTED RS485 COMMAND FORMATS:
1. Text-based: "<slave_id> <color>"
   - Example: "2 red" → Sets all LEDs to red on slave 2
   - Colors supported: red, green, blue, yellow, white, off

2. Binary (6 bytes): [slave_id, box, pattern, R, G, B]
   - Lights up a specific box (25 LEDs) with RGB values
   - Used for tile-based animation (e.g., chaser)

🌐 WEB FEATURES:
- Access via IP or mDNS hostname (e.g., http://slave2.local).
- WebSocket-based live log console.
- Input field for sending manual commands.
- OTA Update page: `/update` for firmware flashing.

⚙️ HARDWARE CONFIGURATION:
- RS485 UART: Serial2 (RX=16, TX=17)
- RS485 Direction Pin: GPIO 4 (controls DE/RE)
- NeoPixel LED Pin: GPIO 5
- Total LEDs: 900 (36 boxes × 25 LEDs)
- Powered by 5V & suitable current source

📂 STORED IN FLASH:
- `id`: Slave ID (retrieved on boot from Preferences)

🧪 TESTING:
- Connect multiple slaves via RS485 bus
- Send commands via Python master or Web UI
- Confirm output in the WebSocket console

📢 NOTE:
- OTA uses built-in Update class (no AsyncElegantOTA)
- Ensure unique slave IDs for each board
- Do not send overlapping RS485 and WebSocket commands

===============================================
*/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Adafruit_NeoPixel.h>

// === CONFIGURATION ===
#define LED_PIN         5
#define LED_COUNT       900

#define RS485_RX_PIN    16
#define RS485_TX_PIN    17
#define RS485_DIR_PIN   4      // Direction control pin (DE+RE on RS485 chip)

#define BOX_SIZE        25
#define NUM_BOXES       (LED_COUNT / BOX_SIZE)
#define BUILD_TIMESTAMP __DATE__ " " __TIME__

const char* ssid = "Gokapture";
const char* password = "P@ssw0rd";

// === OBJECTS ===
Preferences prefs;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
HardwareSerial RS485Serial(2); // UART2

uint8_t slave_id = 0;

// === UTILITIES ===
void notifyClients(const String &msg) {
  ws.textAll(msg + "\n");
}

void logBootInfo() {
  notifyClients("🔌 LED Slave Booted");
  notifyClients("Slave ID: " + String(slave_id));
  notifyClients("IP Address: " + WiFi.localIP().toString());
  notifyClients("Hostname: slave" + String(slave_id) + ".local");
  notifyClients("Firmware Build: " + String(BUILD_TIMESTAMP));
}

// === LED CONTROL ===
void setStripColor(String color) {
  uint32_t c;
  if      (color.equalsIgnoreCase("red"))    c = strip.Color(255, 0, 0);
  else if (color.equalsIgnoreCase("green"))  c = strip.Color(0, 255, 0);
  else if (color.equalsIgnoreCase("blue"))   c = strip.Color(0, 0, 255);
  else if (color.equalsIgnoreCase("white"))  c = strip.Color(255, 255, 255);
  else if (color.equalsIgnoreCase("yellow")) c = strip.Color(255, 255, 0);
  else if (color.equalsIgnoreCase("off"))    c = strip.Color(0, 0, 0);
  else return;

  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}

void lightUpBox(uint8_t boxIndex, uint8_t r, uint8_t g, uint8_t b) {
  if (boxIndex >= NUM_BOXES) return;
  int start = boxIndex * BOX_SIZE;
  int end = start + BOX_SIZE;
  for (int i = start; i < end; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
//  strip.show();
  notifyClients("🎯 Box " + String(boxIndex) + " → RGB(" + r + "," + g + "," + b + ")");
}

// === PARSE & EXECUTE COMMAND ===
void parseCommand(String msg) {
  msg.trim();
  int spaceIndex = msg.indexOf(' ');
  if (spaceIndex == -1) return;

  String idStr = msg.substring(0, spaceIndex);
  String color = msg.substring(spaceIndex + 1);
  idStr.trim();
  color.trim();

  if (idStr.toInt() == slave_id) {
    notifyClients("✅ Accepted Command: " + msg);
    setStripColor(color);
  } else {
    notifyClients("⛔ Ignored Command: " + msg);
  }
}

// === OTA UPDATE PAGE ===
void setupOTA() {
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", R"rawliteral(
      <!DOCTYPE html><html><head><title>OTA Update</title>
      <style>body{font-family:Arial;text-align:center;padding:50px;}input,a{padding:10px 20px;margin:10px;border-radius:5px;background:#4CAF50;color:#fff;text-decoration:none;border:none;}a{background:#2196F3;}</style></head><body>
      <h1>OTA Firmware Update</h1>
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <input type='file' name='update'><br>
        <input type='submit' value='Upload & Update'>
      </form>
      <a href="/">Back to Console</a>
      </body></html>)rawliteral");
  });

  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *request){
      if (Update.hasError())
        request->send(500, "text/plain", "❌ Update Failed");
      else
        request->send(200, "text/plain", "✅ Update successful. Rebooting...");
      delay(1000);
      ESP.restart();
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if (!index && !Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      if (!Update.hasError() && Update.write(data, len) != len) Serial.println("Write failed!");
      if (final && Update.end(true)) Serial.printf("Update Success: %u bytes\n", index + len);
      else if (final) Update.printError(Serial);
    }
  );
}

// === WEB PAGE UI ===
void setupWebPage() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"rawliteral(
      <!DOCTYPE html><html><head><meta charset="utf-8"><title>)rawliteral"
      + String("LED Slave " + String(slave_id)) + R"rawliteral( Console</title>
      <style>body{font-family:Arial;background:#f9f9f9;color:#333;display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh;margin:0;}#log{background:#eee;border:1px solid #ccc;padding:15px;width:90%;max-height:55vh;overflow-y:auto;border-radius:6px;white-space:pre-wrap;}button,a,input[type="text"]{margin-top:10px;padding:10px 20px;border-radius:5px;font-size:16px;border:none;}button,a{background:#2196F3;color:#fff;cursor:pointer;text-decoration:none;}input[type="text"]{width:60%;border:1px solid #aaa;}</style>
      </head><body><h1>LED Slave Console</h1>
      <div id="log">Booting...\n</div>
      <input type="text" id="msgInput" placeholder="Type message like '1 red'...">
      <button onclick="sendMsg()">Send</button>
      <button onclick="clearLog()">Clear Log</button>
      <a href="/update">OTA Update</a>
      <script>
        const log = document.getElementById("log");
        const ws = new WebSocket("ws://" + location.host + "/ws");
        ws.onmessage = e => { log.textContent += e.data + "\n"; log.scrollTop = log.scrollHeight; };
        function sendMsg() {
          let msg = document.getElementById("msgInput").value;
          if (msg) ws.send(msg);
          document.getElementById("msgInput").value = "";
        }
        function clearLog() { log.textContent = ""; }
      </script></body></html>
    )rawliteral";

    request->send(200, "text/html", html);
  });
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  pinMode(RS485_DIR_PIN, OUTPUT);
  digitalWrite(RS485_DIR_PIN, LOW); // Start in receive mode

  strip.begin();
  strip.show();

  prefs.begin("slave", true);
  slave_id = prefs.getUChar("id", 0);
  prefs.end();

  RS485Serial.begin(115200, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  delay(2000); // Let RS485 & master settle

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  String hostname = "slave" + String(slave_id);
  MDNS.begin(hostname.c_str());

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      client->text("Connected to LED Slave " + String(slave_id));
      logBootInfo();
    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        String msg = "";
        for (size_t i = 0; i < len; i++) msg += (char)data[i];
        notifyClients("Received (WS): " + msg);
        parseCommand(msg);
      }
    }
  });

  server.addHandler(&ws);
  setupOTA();
  setupWebPage();
  server.begin();

  notifyClients("✅ ESP32 Slave Ready");
}

// === LOOP ===
void loop() {
  static String rs485Buffer = "";
  bool updated = false;

  // Handle text messages
  while (RS485Serial.available()) {
    char c = RS485Serial.peek();
    if (c == '\n') {
      RS485Serial.read(); // remove '\n'
      rs485Buffer.trim();
      if (rs485Buffer.length() > 0) {
        notifyClients("Received (RS485): " + rs485Buffer);
        parseCommand(rs485Buffer);
      }
      rs485Buffer = "";
    } else if (RS485Serial.available() >= 6) {
      // Check for 6-byte binary command
      uint8_t packet[6];
      RS485Serial.readBytes(packet, 6);
      if (packet[0] == slave_id) {
        uint8_t box = packet[1];
        uint8_t r = packet[3];
        uint8_t g = packet[4];
        uint8_t b = packet[5];
        lightUpBox(box, r, g, b);
        updated = true;  
      }
    } else {
      char c = RS485Serial.read();
      if (c == '\n') {
        rs485Buffer.trim();
        if (rs485Buffer.length() > 0) {
          notifyClients("Received (RS485): " + rs485Buffer);
          parseCommand(rs485Buffer);
        }
        rs485Buffer = "";
      } else {
        rs485Buffer += c;
      }
    }
  }

  if (updated) strip.show();
}
