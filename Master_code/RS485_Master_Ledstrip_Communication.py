"""
===============================================================================
RS485 Master Controller - Python Script
-------------------------------------------------------------------------------
This script enables RS485 communication between a master (e.g., Raspberry Pi or PC)
and ESP32 slave devices over serial. It allows the user to:

✅ Send color commands to specific slave IDs (e.g., "1 red")  
📥 Receive messages from slaves  
📂 Validate slave IDs using a CSV mapping file (mac_map.csv)

-------------------------------------------------------------------------------
REQUIREMENTS:
- Python 3.6 or higher
- pyserial (`pip install pyserial`)
- RS485 USB-to-Serial adapter connected to the correct serial port
- CSV file named 'mac_map.csv' in the same directory

-------------------------------------------------------------------------------
CSV FORMAT (mac_map.csv):
    mac,id
    A8:42:E3:12:34:56,1
    B4:E6:2D:78:90:AB,2
    ...

Only slave IDs listed in the CSV will be accepted as valid targets.

-------------------------------------------------------------------------------
USAGE:
1. Run the script:
       python rs485_master.py

2. Type a command:
       <id> <color>
       Example:  2 red

3. Incoming messages from slaves will be printed automatically.

-------------------------------------------------------------------------------
CONFIGURATION:
- Modify `PORT` and `BAUDRATE` at the top of this file to match your setup.

-------------------------------------------------------------------------------
EXIT:
- Use Ctrl+C to exit safely. The serial port will close automatically.

-------------------------------------------------------------------------------
AUTHOR: YourNameHere
LICENSE: MIT
===============================================================================
"""

import serial
import threading
import csv
import os

PORT = '/dev/ttyUSB1'  # Change this to match your system
BAUDRATE = 115200
CSV_FILE = 'mac_map.csv'

# Load valid IDs from CSV
def load_valid_ids():
    ids = set()
    if not os.path.exists(CSV_FILE):
        print(f"❌ CSV file '{CSV_FILE}' not found.")
        return ids
    try:
        with open(CSV_FILE, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                try:
                    ids.add(int(row['id']))
                except (ValueError, KeyError):
                    continue
    except Exception as e:
        print(f"⚠ Error reading CSV: {e}")
    return ids

# Open serial connection
try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=0.1)
except Exception as e:
    print(f"❌ Failed to open serial port: {e}")
    exit()

valid_ids = load_valid_ids()

# Thread: Receive messages from slaves
def read_loop():
    print("📥 Listening for RS485 slave messages...")
    while True:
        try:
            data = ser.readline()
            if data:
                print("📥 Slave:", data.decode('utf-8', errors='ignore').strip())
        except Exception as e:
            print("❌ Error reading:", e)

# Thread: Send messages to slaves
def write_loop():
    print("✍ Format: <id> <color> (e.g., '1 red')")
    while True:
        try:
            msg = input("Send> ").strip()
            if not msg:
                continue

            parts = msg.split(' ', 1)
            if len(parts) != 2 or not parts[0].isdigit():
                print("⚠ Format should be: <id> <color>")
                continue

            target_id = int(parts[0])
            color = parts[1].strip()

            if target_id not in valid_ids:
                print(f"❌ Invalid ID: {target_id} (Not found in '{CSV_FILE}')")
                continue

            final_msg = f"{target_id} {color}\n"
            ser.write(final_msg.encode('utf-8'))
            print(f"✅ Sent to ID {target_id}: {color}")

        except KeyboardInterrupt:
            print("\n👋 Exiting...")
            break
        except Exception as e:
            print("❌ Error sending:", e)

# Main
if _name_ == "_main_":
    try:
        threading.Thread(target=read_loop, daemon=True).start()
        write_loop()
    finally:
        ser.close()