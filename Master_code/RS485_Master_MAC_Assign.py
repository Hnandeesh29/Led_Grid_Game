"""
RS485 Master – ESP32 MAC-Based ID Assignment Script
----------------------------------------------------
Version    : 2.0
Date       : 2025-07-17
Language   : Python 3
Description:
    - Listens to RS485 for MAC addresses (6 bytes) from ESP32 slaves
    - Assigns unique slave ID (1–255) to each MAC if not already assigned
    - Sends 7-byte response: [MAC(6 bytes) + ID(1 byte)]
    - Saves MAC-to-ID mapping in 'mac_map.csv'
    - Prevents duplicate ID or MAC assignment

Usage:
    - Connect USB-to-RS485 adapter to Raspberry Pi or PC
    - Adjust PORT to match your system (e.g., "COM3" on Windows or "/dev/ttyUSB0" on Linux)
    - Run the script with Python 3: python master.py

CSV File:
    - Stored as mac_map.csv in the current directory
    - Format: mac,id

Author     : [Your Name or Team Name]
"""

import serial
import time
import csv
import os

PORT = "/dev/ttyUSB0"  # Change this to your RS485 USB port
BAUDRATE = 115200
CSV_FILE = "mac_map.csv"

def load_mac_map():
    mac_map = {}
    if os.path.exists(CSV_FILE):
        with open(CSV_FILE, mode='r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                if "mac" in row and "id" in row:
                    mac_map[row["mac"].upper()] = int(row["id"])
    return mac_map

def save_mac_id(mac_str, assigned_id):
    file_exists = os.path.exists(CSV_FILE)
    with open(CSV_FILE, mode='a', newline='') as f:
        writer = csv.writer(f)
        if not file_exists or os.path.getsize(CSV_FILE) == 0:
            writer.writerow(["mac", "id"])
        writer.writerow([mac_str, assigned_id])

def get_next_available_id(mac_map):
    used_ids = set(mac_map.values())
    for i in range(1, 256):
        if i not in used_ids:
            return i
    return None  # All 255 IDs used

def main():
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    mac_map = load_mac_map()
    print("🔌 Master ready. Waiting for MACs from slaves...\n")

    while True:
        if ser.in_waiting >= 6:
            mac_bytes = ser.read(6)
            mac_str = ':'.join(f'{b:02X}' for b in mac_bytes)
            print(f"📥 Received MAC: {mac_str}")

            if mac_str in mac_map:
                print(f"⚠ Already assigned (ID: {mac_map[mac_str]}), skipping.\n")
                continue

            new_id = get_next_available_id(mac_map)
            if new_id is None:
                print("❌ No available IDs left (1–255).")
                continue

            packet = mac_bytes + bytes([new_id])
            ser.write(packet)
            save_mac_id(mac_str, new_id)
            mac_map[mac_str] = new_id
            print(f"✅ Assigned ID {new_id} to {mac_str} and saved to CSV.\n")

        time.sleep(0.1)

if _name_ == "_main_":
    main()