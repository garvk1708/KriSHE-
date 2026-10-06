import serial
import time
import re
import os
import csv
from datetime import datetime

COM_PORT = 'COM9'
BAUD_RATE = 115200
CSV_FILE = os.path.join(os.getcwd(), 'krishe_telemetry_live.csv')

print(f"Starting continuous CSV telemetry collection on {COM_PORT}...")
print(f"Saving real-time data to: {CSV_FILE}")

file_exists = os.path.exists(CSV_FILE) and os.path.getsize(CSV_FILE) > 0

with open(CSV_FILE, mode='a', newline='', encoding='utf-8') as f:
    writer = csv.writer(f)
    if not file_exists:
        writer.writerow([
            "Timestamp_ISO",
            "Uptime_s",
            "State",
            "Chip_Die_C",
            "Top_Temp_C",
            "Top_Valid",
            "Mid_Temp_C",
            "Mid_Valid",
            "Bot_Temp_C",
            "Bot_Valid",
            "Satellites",
            "Lat",
            "Lon"
        ])
        f.flush()

    try:
        ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=2)
        count = 0
        current_state = "UNKNOWN"
        uptime_s = 0
        chip_die = 0.0
        top_temp = 0.0
        top_valid = 0
        mid_temp = 0.0
        mid_valid = 0
        bot_temp = 0.0
        bot_valid = 0
        sats = 0
        lat = 0.0
        lon = 0.0

        print("Connected to COM9. Collecting telemetry samples...")

        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue

            # Parse log format
            # UPTIME: 131 s | STATE: PREHEATING | CHIP DIE: 41.6 °C
            if "UPTIME:" in line and "STATE:" in line:
                m = re.search(r'UPTIME:\s*(\d+)\s*s\s*\|\s*STATE:\s*(\w+)\s*\|\s*CHIP DIE:\s*([\d\.]+)', line)
                if m:
                    uptime_s = int(m.group(1))
                    current_state = m.group(2)
                    chip_die = float(m.group(3))

            # TOP (CS7): 60.75 °C
            if "TOP" in line and "CS" in line:
                if "OPEN" in line or "FAILD" in line:
                    top_temp = 0.0
                    top_valid = 0
                else:
                    m = re.search(r'TOP\s*\([^)]+\):\s*([\d\.-]+)\s*°C', line)
                    if m:
                        top_temp = float(m.group(1))
                        top_valid = 1

            # MIDDLE (CS15): 54.25 °C
            if "MIDDLE" in line and "CS" in line:
                if "OPEN" in line or "FAILD" in line:
                    mid_temp = 0.0
                    mid_valid = 0
                else:
                    m = re.search(r'MIDDLE\s*\([^)]+\):\s*([\d\.-]+)\s*°C', line)
                    if m:
                        mid_temp = float(m.group(1))
                        mid_valid = 1

            # BOTTOM (CS16):
            if "BOTTOM" in line and "CS" in line:
                if "OPEN" in line or "FAILD" in line:
                    bot_temp = 0.0
                    bot_valid = 0
                else:
                    m = re.search(r'BOTTOM\s*\([^)]+\):\s*([\d\.-]+)\s*°C', line)
                    if m:
                        bot_temp = float(m.group(1))
                        bot_valid = 1

            # GNSS parsing
            if "STATUS:" in line:
                m_sats = re.search(r'(\d+)\s*used in fix', line)
                if m_sats:
                    sats = int(m_sats.group(1))
                else:
                    m_lock = re.search(r'3D Lock with (\d+) sats', line)
                    if m_lock:
                        sats = int(m_lock.group(1))

            if "LATITUDE:" in line:
                m_lat = re.search(r'LATITUDE:\s*([\d\.-]+)', line)
                if m_lat:
                    lat = float(m_lat.group(1))

            if "LONGITUDE:" in line:
                m_lon = re.search(r'LONGITUDE:\s*([\d\.-]+)', line)
                if m_lon:
                    lon = float(m_lon.group(1))

                # Every time BOTTOM frame finishes, record a complete CSV row
                now_str = datetime.now().isoformat()
                writer.writerow([
                    now_str,
                    uptime_s,
                    current_state,
                    chip_die,
                    top_temp,
                    top_valid,
                    mid_temp,
                    mid_valid,
                    bot_temp,
                    bot_valid,
                    sats,
                    lat,
                    lon
                ])
                f.flush()
                count += 1
                print(f"[{count}] Saved sample @ {now_str}: Top={top_temp}°C, Mid={mid_temp}°C, Bot={bot_temp}°C, State={current_state}")

    except KeyboardInterrupt:
        print("Data collection stopped by user.")
    except Exception as e:
        print(f"Serial logger error: {e}")
