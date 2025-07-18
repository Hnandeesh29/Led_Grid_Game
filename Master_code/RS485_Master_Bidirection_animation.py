# 🔁 Bidirectional Red Chaser Game – RS485 LED Controller (Python)

This Python script controls a set of ESP32 "slave" LED controllers over **RS485 serial** to run a **bidirectional red chaser** animation. The animation scrolls across multiple rows of LED boxes (forward and backward), with randomly assigned **green** and **blue** special tiles.

---

## 🧩 System Overview

- 8 ESP32 "slave" controllers
- Each slave controls **12 boxes**
- Each box contains **25 WS2812 LEDs**
- The script uses **RS485 serial protocol** to send **6-byte commands** to set solid RGB colors per box
- Animation flows **forward and backward**, row by row

---

## 🔧 Hardware Setup

| Component         | Description                                  |
|------------------|----------------------------------------------|
| ESP32 Slaves     | Each handles 12 boxes of WS2812 LEDs         |
| RS485 Network     | Shared bus with DE/RE control on GPIO        |
| USB-RS485 Adapter | Connects PC to RS485 (e.g., `/dev/ttyUSB1`) |
| LED Power Supply | External 5V power for LED strips              |

---

## 🎨 LED Box Color Roles

| Role    | Color         | Behavior                             |
|---------|---------------|--------------------------------------|
| NONE    | RED (chased)  | Default chased box color             |
| GREEN   | Green         | Always stays green                   |
| BLUE    | Orange → Blue | Flashes orange on chase, then blue   |

- On each run:
  - **3 boxes** are randomly assigned as GREEN
  - **2 boxes** as BLUE

---

## 🧾 Command Protocol

Each LED box receives a **6-byte binary packet**:

```text
[slave_id, local_box_id, pattern_id, R, G, B]

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

# === Randomly assign 3 green and 2 blue boxes ===
tile_roles = [ROLE_NONE] * TOTAL_BOXES
special_indices = random.sample(range(TOTAL_BOXES), 5)
for i in special_indices[:3]:
    tile_roles[i] = ROLE_GREEN
for i in special_indices[3:]:
    tile_roles[i] = ROLE_BLUE

# === TIMING CONFIG ===
CHASE_DELAY = 0.2
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

def red_chaser_game(ser):
    num_slaves = len(BOXES_PER_SLAVE)
    current_slave_idx = 0
    direction = 1  # 1 = forward, -1 = backward
    previous_blue_boxes = []

    print("🚀 Bidirectional red chaser started with 3 GREEN tiles and 2 BLUE tiles")

    try:
        while True:
            box_start = sum(BOXES_PER_SLAVE[:current_slave_idx])
            box_end = box_start + BOXES_PER_SLAVE[current_slave_idx]
            row_boxes = list(range(box_start, box_end))

            # Step 1: Restore previously orange (blue) tiles to blue (from prior row)
            for i in previous_blue_boxes:
                send_box_color(ser, i, COLOR_BLUE)
            previous_blue_boxes = []

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

            # Step 4: Update direction
            if direction == 1 and current_slave_idx == num_slaves - 1:
                direction = -1
            elif direction == -1 and current_slave_idx == 0:
                direction = 1

            current_slave_idx += direction

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