/*
ESP32 RS485 Slave – WebSocket Console & Web OTA
------------------------------------------------

Version    : 1.0  
Date       : 2025-07-17  
Platform   : ESP32 (Arduino Framework)

Description:
  - Receives RS485 messages from master via Serial1
  - Forwards matching messages (based on slave ID) to WebSocket log console
  - Sends user input from Serial or WebSocket to RS485 master
  - Displays real-time logs on browser console
  - Supports bidirectional RS485 communication
  - Stores slave ID in internal flash using Preferences
  - Hosts OTA update page accessible via browser
  - Provides mDNS access via http://slave<ID>.local
  - Shows all boot info in WebSocket console on client connect

Pin Mapping:
  - RS485_TXD    : GPIO 17  
  - RS485_RXD    : GPIO 16  
  - RS485_RE_DE  : GPIO 4

Web Interface:
  - WebSocket Console: http://slave<ID>.local/
  - OTA Update Page  : http://slave<ID>.local/update
  - Features:
      • Live log viewer with auto-scroll
      • Message input box (sends to RS485)
      • Log clear button
      • Navigation to OTA upload page

Dependencies:
  - WiFi.h  
  - AsyncTCP.h  
  - ESPAsyncWebServer.h  
  - Preferences.h  
  - ESPmDNS.h  
  - Update.h

Author     : NA
*/



#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <Update.h>

#define RS485_TXD    17
#define RS485_RXD    16
#define RS485_RE_DE  4
#define BAUD_RATE    115200

const char* ssid = "Gokapture";
const char* password = "P@ssw0rd";

Preferences prefs;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

uint8_t slave_id = 0;
char buffer[128];

void enableTx() {
  digitalWrite(RS485_RE_DE, HIGH);
  delay(1);
}

void enableRx() {
  digitalWrite(RS485_RE_DE, LOW);
  delay(1);
}

void notifyClients(const String &msg) {
  ws.textAll(msg + "\n");
}

void logBootInfo() {
  notifyClients("RS485 Slave Console Booted");
  notifyClients("Slave ID: " + String(slave_id));
  notifyClients("IP Address: " + WiFi.localIP().toString());
  notifyClients("Hostname: slave" + String(slave_id) + ".local");
}

void handleRS485() {
  while (Serial1.available()) {
    int len = Serial1.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    if (len > 0) {
      buffer[len] = '\0';
      String message = String(buffer);
      Serial.println("Received from Master: " + message);

      // Only forward messages that match this slave's ID
      // New: Allow space format like "1 hi"
      if (message.startsWith(String(slave_id) + " ") || message.startsWith(String(slave_id) + ":")) {
          notifyClients("RS485 IN: " + message);
      }

    }
  }
}

void handleUSBToRS485() {
  while (Serial.available()) {
    int len = Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    if (len > 0) {
      buffer[len] = '\0';
      String message = String(buffer);
      Serial.println("Sending to Master: " + message);
      notifyClients("RS485 OUT: " + message);
      enableTx();
      Serial1.println(message);
      Serial1.flush();
      enableRx();
    }
  }
}

void setupOTA() {
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", R"rawliteral(
      <!DOCTYPE html><html><head><title>OTA Update</title>
      <style>
        body { font-family: Arial; text-align: center; padding: 50px; }
        input[type='file'], input[type='submit'], a {
          padding: 10px 20px; margin: 10px; border-radius: 5px;
          background-color: #4CAF50; color: white; border: none;
          cursor: pointer; text-decoration: none;
        }
        a { background-color: #2196F3; }
      </style></head><body>
      <h1>OTA Firmware Update</h1>
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <input type='file' name='update'><br>
        <input type='submit' value='Upload & Update'>
      </form>
      <a href="/">Back to Console</a>
      </body></html>
    )rawliteral");
  });

  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      request->send(200, "text/html", "<h1>Update Done. Rebooting...</h1>");
      delay(1000);
      ESP.restart();
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if (!index && !Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      if (!Update.hasError()) Update.write(data, len);
      if (final && Update.end(true)) Serial.printf("OTA Success: %u bytes\n", index + len);
    }
  );
}

void setupWebPage() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String title = "RS485 Slave " + String(slave_id) + " Console";
    String html = R"rawliteral(
      <!DOCTYPE html><html><head><meta charset="utf-8">
      <title>)rawliteral" + title + R"rawliteral(</title>
      <style>
        body {
          font-family: Arial; background: #f9f9f9; color: #333;
          display: flex; flex-direction: column; align-items: center;
          justify-content: center; height: 100vh; margin: 0;
        }
        h1 { margin-bottom: 10px; }
        #log {
          background: #eee; border: 1px solid #ccc; padding: 15px;
          width: 90%; max-height: 55vh; overflow-y: auto;
          border-radius: 6px; white-space: pre-wrap;
        }
        button, a, input[type="text"] {
          margin-top: 10px; padding: 10px 20px; border-radius: 5px;
          font-size: 16px; border: none;
        }
        button, a {
          background: #2196F3; color: white; cursor: pointer;
          text-decoration: none;
        }
        input[type="text"] {
          width: 60%;
          border: 1px solid #aaa;
        }
      </style></head><body>
        <h1>)rawliteral" + title + R"rawliteral(</h1>
        <div id="log">Booting...\n</div>
        <input type="text" id="msgInput" placeholder="Type message here...">
        <button onclick="sendMsg()">Send</button>
        <button onclick="clearLog()">Clear Log</button>
        <a href="/update">OTA Update</a>
        <script>
          const log = document.getElementById("log");
          const ws = new WebSocket("ws://" + location.host + "/ws");

          ws.onmessage = e => {
            log.textContent += e.data + "\n";
            log.scrollTop = log.scrollHeight;
          };

          function sendMsg() {
            let msg = document.getElementById("msgInput").value;
            if (msg) ws.send(msg);
            document.getElementById("msgInput").value = "";
          }

          function clearLog() {
            log.textContent = "";
          }
        </script>
      </body></html>
    )rawliteral";

    request->send(200, "text/html", html);
  });
}

void setup() {
  Serial.begin(115200);
  pinMode(RS485_RE_DE, OUTPUT);
  enableRx();
  Serial1.begin(BAUD_RATE, SERIAL_8N1, RS485_RXD, RS485_TXD);

  prefs.begin("slave", true);
  slave_id = prefs.getUChar("id", 0);
  prefs.end();
  Serial.println("Slave ID: " + String(slave_id));

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected. IP: " + WiFi.localIP().toString());

  String hostname = "slave" + String(slave_id);
  if (MDNS.begin(hostname.c_str())) {
    Serial.println("mDNS started: http://" + hostname + ".local");
  }

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      client->text("Connected to RS485 Slave " + String(slave_id));
      logBootInfo();
    }
    else if (type == WS_EVT_DATA) {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        String msg = "";
        for (size_t i = 0; i < len; i++) msg += (char)data[i];
        notifyClients("RS485 OUT (WS): " + msg);
        Serial.println("Sending (WS) to Master: " + msg);
        enableTx();
        Serial1.println(msg);
        Serial1.flush();
        enableRx();
      }
    }
  });

  server.addHandler(&ws);
  setupOTA();
  setupWebPage();
  server.begin();
}

void loop() {
  handleRS485();
  handleUSBToRS485();
}
