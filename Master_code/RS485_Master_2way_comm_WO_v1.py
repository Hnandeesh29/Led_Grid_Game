"""
RS485 Master Console – Validated Command Sender
------------------------------------------------

Version    : 1.0  
Date       : 2025-07-17  
Language   : Python 3

Description:
  - Opens serial connection to RS485 network
  - Reads valid slave IDs from 'mac_map.csv'
  - Starts a background thread to listen for incoming RS485 messages
  - Accepts user input from terminal in format: <id> <message>
  - Validates input against known slave IDs
  - Sends command over RS485 only if ID exists in CSV
  - Prints real-time responses from slaves to terminal

CSV Format:
  - File: mac_map.csv (must exist in the same directory)
  - Columns: mac,id
  - Example: C6:FE:24:9A:F0:6F,1

Dependencies:
  - Python 3  
  - pyserial (`pip install pyserial`)

Usage:
  - Update PORT value to match your RS485 USB port (e.g., /dev/ttyUSB0 or COM3)
  - Run the script:
      python3 master_console.py
  - Type messages like:
      1 LED ON
      2 start

Author     : [Your Name or Team Name]
"""
import serial
import threading
import csv
import os

PORT = '/dev/ttyUSB0'  # Update this if different (e.g., COM3 on Windows)
BAUDRATE = 115200
CSV_FILE = 'mac_map.csv'

# Load valid IDs from mac_map.csv
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

# Receive messages from slaves
def read_loop():
    print("📥 Listening for incoming RS485 messages...")
    while True:
        try:
            data = ser.readline()
            if data:
                print("📥 From slave:", data.decode('utf-8', errors='ignore').strip())
        except Exception as e:
            print("❌ Error reading from serial:", e)

# Send messages to slaves
def write_loop():
    print("✍ Type messages in format: <id> <message> (e.g., '1 LED ON')")
    while True:
        try:
            msg = input("Send> ").strip()
            if not msg:
                continue

            parts = msg.split(' ', 1)
            if len(parts) != 2 or not parts[0].isdigit():
                print("⚠ Format should be: <id> <message> (e.g., '2 Hello')")
                continue

            target_id = int(parts[0])
            message = parts[1]

            if target_id not in valid_ids:
                print(f"❌ Invalid slave ID: {target_id} (Not found in '{CSV_FILE}')")
                continue

            ser.write(f"{target_id} {message}\n".encode('utf-8'))

        except KeyboardInterrupt:
            print("\n👋 Exiting...")
            break
        except Exception as e:
            print("❌ Error sending message:", e)

# Main entry
if _name_ == "_main_":
    try:
        threading.Thread(target=read_loop, daemon=True).start()
        write_loop()
    finally:
        ser.close()