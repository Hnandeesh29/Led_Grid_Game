# 🔴 Red Chaser Game – RS485 LED Control (Python Script)

This Python script controls a distributed LED system of ESP32 "slaves" over RS485, running a **Red Chaser Game** pattern across multiple boxes of LEDs. It uses a binary protocol to communicate and light up specific LED boxes in patterns based on their roles (normal, green, or blue).

---

## 🧩 System Overview

- Each **slave** controls 12 "boxes" of 25 LEDs.
- There are **8 slaves** (total of 96 boxes).
- Communication is done over **RS485** via a USB–RS485 adapter.
- LED boxes can be assigned special roles (`GREEN`, `BLUE`, or `NONE`).
- The game runs a "red chaser" animation, row by row.
- Special tiles:
  - `GREEN` tiles stay green.
  - `BLUE` tiles flash orange when the red chaser passes.

---

## 🛠️ Hardware Requirements

- ESP32-based boards running compatible slave firmware.
- WS2812 or compatible LED strips (25 LEDs per box).
- USB–RS485 converter (connected to your PC).
- RS485 wiring between all ESP32 slaves.
- Power supply sufficient for all LEDs.

---

## ⚙️ Configuration

- `PORT`: Serial port of your USB–RS485 adapter (e.g., `/dev/ttyUSB1` or `COM4`)
- `BAUD`: Serial baud rate (`115200`)
- `BOXES_PER_SLAVE`: List representing number of boxes per slave
- `LEDS_PER_BOX`: Fixed at `25`
- `PATTERN_SOLID`: ID for the "solid color" pattern (must match firmware)

---

## 🎨 Color Codes

| Name     | RGB         | Usage                        |
|----------|-------------|------------------------------|
| `OFF`    | `(0, 0, 0)` | Default/off state            |
| `RED`    | `(255, 0, 0)` | Main red chaser             |
| `GREEN`  | `(0, 255, 0)` | Special green tiles         |
| `BLUE`   | `(0, 0, 255)` | Special blue tiles (default)|
| `ORANGE` | `(255, 128, 0)` | Flashing color for blue tiles|

---

## 🎮 Game Mechanics

1. **Random Roles**:
   - On each script run, 3 tiles are randomly chosen as `GREEN`, 2 as `BLUE`.
2. **Red Chaser**:
   - Iterates row-by-row (slave-by-slave), coloring:
     - Normal tiles in red.
     - Green tiles always green.
     - Blue tiles temporarily orange.
3. **Restoration**:
   - Blue tiles revert from orange to blue in the next frame.

---

## 🧪 How to Run

1. Plug in USB–RS485 adapter and ensure slaves are powered.
2. Edit `PORT = "/dev/ttyUSB1"` to match your system.
3. Install Python requirements (if any, e.g., `pyserial`):
   ```bash
   pip install pyserial


import serial
import struct
import time
import random

# === CONFIGURATION ===
PORT = "/dev/ttyUSB1"
BAUD = 115200
BOXES_PER_SLAVE = [12] * 8  # 8 slaves, 12 boxes each
TOTAL_BOXES = sum(BOXES_PER_SLAVE)
LEDS_PER_BOX = 25
PATTERN_SOLID = 1

# === COLORS ===
COLOR_OFF     = (0, 0, 0)
COLOR_RED     = (255, 0, 0)
COLOR_GREEN   = (0, 255, 0)
COLOR_BLUE    = (0, 0, 255)
COLOR_YELLOW  = (255, 255, 0)
COLOR_ORANGE  = (255, 128, 0)

# === TILE ROLES ===
ROLE_NONE  = "NONE"
ROLE_GREEN = "GREEN"
ROLE_BLUE  = "BLUE"

# === Initialize tile roles (3 green, 2 blue randomly)
tile_roles = [ROLE_NONE] * TOTAL_BOXES
special_indices = random.sample(range(TOTAL_BOXES), 5)
for i in special_indices[:3]:
    tile_roles[i] = ROLE_GREEN
for i in special_indices[3:]:
    tile_roles[i] = ROLE_BLUE

# === TIMING CONFIG ===
CHASE_DELAY = 0.4
FLASH_TIME = 0.4

def get_slave_and_local_box(box_idx):
    remaining = box_idx
    for slave_id, count in enumerate(BOXES_PER_SLAVE, start=1):
        if remaining < count:
            return slave_id, remaining
        remaining -= count
    raise ValueError("Invalid box index")

def send_box_color(ser, box_idx, rgb):
    slave_id, local_box = get_slave_and_local_box(box_idx)
    r, g, b = rgb
    packet = struct.pack("BBBBBB", slave_id, local_box, PATTERN_SOLID, r, g, b)
    ser.write(packet)
    time.sleep(0.002)

def update_normal_state(ser, box_states):
    for i in range(TOTAL_BOXES):
        role = tile_roles[i]
        if box_states[i] == "YELLOW":
            send_box_color(ser, i, COLOR_YELLOW)
        elif role == ROLE_GREEN:
            send_box_color(ser, i, COLOR_GREEN)
        elif role == ROLE_BLUE:
            send_box_color(ser, i, COLOR_BLUE)
        else:
            send_box_color(ser, i, COLOR_OFF)

def red_chaser_game(ser):
    current_slave_idx = 0
    num_slaves = len(BOXES_PER_SLAVE)

    print("🚀 Red chaser started with 3 GREEN tiles and 2 BLUE tiles")

    try:
        previous_blue_boxes = []

        while True:
            box_start = sum(BOXES_PER_SLAVE[:current_slave_idx])
            box_end = box_start + BOXES_PER_SLAVE[current_slave_idx]
            row_boxes = list(range(box_start, box_end))

            # Step 1: Restore previously orange (blue) tiles to blue (from prior row)
            for i in previous_blue_boxes:
                send_box_color(ser, i, COLOR_BLUE)
            previous_blue_boxes = []  # Clear for current row

            # Step 2: Turn off all other tiles outside the current row
            for i in range(TOTAL_BOXES):
                if i in row_boxes:
                    continue
                role = tile_roles[i]
                if role == ROLE_GREEN:
                    send_box_color(ser, i, COLOR_GREEN)
                elif role == ROLE_BLUE:
                    send_box_color(ser, i, COLOR_BLUE)
                else:
                    send_box_color(ser, i, COLOR_OFF)

            # Step 3: Paint current row
            for i in row_boxes:
                role = tile_roles[i]

                if role == ROLE_GREEN:
                    send_box_color(ser, i, COLOR_GREEN)

                elif role == ROLE_BLUE:
                    send_box_color(ser, i, COLOR_ORANGE)
                    previous_blue_boxes.append(i)  # Mark to restore next round

                else:
                    send_box_color(ser, i, COLOR_RED)

            time.sleep(CHASE_DELAY)
            current_slave_idx = (current_slave_idx + 1) % num_slaves

    except KeyboardInterrupt:
        print("🛑 Stopping game. Turning off all LEDs.")
        for i in range(TOTAL_BOXES):
            send_box_color(ser, i, COLOR_OFF)
        ser.close()

# === MAIN ===
def main():
    ser = serial.Serial(PORT, BAUD, timeout=1)
    red_chaser_game(ser)

if _name_ == "_main_":
    main()