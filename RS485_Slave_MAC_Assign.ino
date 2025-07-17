/**
 * =============================================================================
 * ESP32 RS485 Slave - MAC-Based ID Assignment
 * -----------------------------------------------------------------------------
 * 📅 Version       : 2.0
 * 🛠 Last Updated  : 2025-07-17
 * 👨‍💻 Author        : Kavarthapu Umamahesh
 * 💡 Description   :
 *   - This ESP32 slave communicates with a master (e.g., Raspberry Pi) via RS485.
 *   - It reads the system MAC using esp_read_mac() and sends it to the master.
 *   - The master replies with a 1-byte unique ID if the MAC matches.
 *   - The ID is saved to non-volatile flash via the Preferences library.
 *   - After ID assignment, the slave uses it for further communication.
 *   - User can reset the ID by sending 'C' over the Serial terminal.
 * 
 * 🔗 Communication:
 *   - RS485 UART1 (TX: GPIO 17, RX: GPIO 16, DE/RE: GPIO 4)
 *   - Baud rate: 115200
 * 
 * 💾 Flash Storage:
 *   - Uses namespace "slave" to store assigned ID persistently.
 * 
 * 🧪 How to Reset ID:
 *   - Open Serial Monitor and type C or c to clear saved ID and reboot.
 * 
 * 📦 Dependencies:
 *   - ESP32 Arduino Core
 *   - No external libraries required
 * 
 * 🧼 Notes:
 *   - MAC is reversed to MSB:LSB for consistency.
 *   - ID range: 1–254 (0 means unassigned, 255 is reserved).
 * =============================================================================
 */

#include <HardwareSerial.h>
#include <Preferences.h>
#include "esp_system.h"  // For esp_read_mac()

#define RS485_RX     16
#define RS485_TX     17
#define RS485_DE_RE  4

HardwareSerial rs485(1);
Preferences prefs;

uint8_t mac[6];              // MAC address (MSB to LSB)
uint8_t slaveId = 0;
bool idAssigned = false;
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 1000;

void enableTransmit() {
  digitalWrite(RS485_DE_RE, HIGH);
  delayMicroseconds(10);
}

void enableReceive() {
  digitalWrite(RS485_DE_RE, LOW);
}

void sendMacToMaster() {
  enableTransmit();
  rs485.write(mac, 6);   // Send MAC in MSB to LSB order
  rs485.flush();
  enableReceive();
  Serial.println("📤 Sent MAC to master");
}

bool receiveIdFromMaster() {
  if (rs485.available() >= 7) {
    uint8_t incoming[7];
    rs485.readBytes(incoming, 7);

    if (memcmp(incoming, mac, 6) == 0) {
      slaveId = incoming[6];
      prefs.begin("slave", false);
      prefs.putUChar("id", slaveId);
      prefs.end();
      Serial.printf("✅ Received and saved ID: %d\n", slaveId);
      return true;
    }
  }
  return false;
}

uint8_t loadId() {
  prefs.begin("slave", true);
  uint8_t id = prefs.getUChar("id", 0);  // Returns 0 if not set
  prefs.end();
  return id;
}

void clearIdAndRestart() {
  prefs.begin("slave", false);
  prefs.remove("id");
  prefs.end();
  Serial.println("🧹 ID cleared. Restarting...");
  delay(1000);
  ESP.restart();
}

void setup() {
  pinMode(RS485_DE_RE, OUTPUT);
  enableReceive();

  Serial.begin(115200);
  rs485.begin(115200, SERIAL_8N1, RS485_RX, RS485_TX);

  // Read system MAC (base MAC) directly
  uint8_t rawMac[6];
  esp_read_mac(rawMac, ESP_MAC_WIFI_STA);  // Returns MAC in LSB to MSB

  // Reverse to MSB:LSB format for transmission
  for (int i = 0; i < 6; i++) {
    mac[i] = rawMac[5 - i];
  }

  // Print MAC for debug
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("🟢 MAC: %s\n", macStr);

  slaveId = loadId();
  if (slaveId > 0 && slaveId < 255) {
    idAssigned = true;
    Serial.printf("✅ Loaded saved ID: %d\n", slaveId);
  } else {
    idAssigned = false;
    slaveId = 0;
    Serial.println("❓ No valid ID found. Requesting from master...");
  }
}

void loop() {
  if (!idAssigned) {
    if (millis() - lastRequestTime > requestInterval) {
      sendMacToMaster();
      lastRequestTime = millis();
    }

    if (receiveIdFromMaster()) {
      idAssigned = true;
      delay(1000);
      ESP.restart();  // Reboot with new ID
    }
    return;
  }

  // Allow clearing ID from serial
  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'c' || ch == 'C') {
      clearIdAndRestart();
    }
  }

  // ✅ Add your logic using slaveId here
}