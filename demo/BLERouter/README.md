# BLERouter

ESP32-C6 BLE scanner that listens for `EnvSensor` advertisements, decodes temperature, humidity, and pressure, and prints each measurement to the serial monitor with a timestamp obtained via NTP at startup.

## How it works

1. On boot, connects to WiFi and syncs time from `pool.ntp.org`.
2. WiFi is stopped before BLE starts (avoids 2.4 GHz coexistence issues).
3. Passively scans for BLE advertisements whose device name is `EnvSensor`.
4. For each matching packet, decodes the Manufacturer Specific Data payload and logs it.

## Setup

### 1. Create the WiFi credentials file

This file is **not tracked by git**. Copy the example and fill in your network details:

```bash
cp main/wifi_credentials.h.example main/wifi_credentials.h
```

Edit `main/wifi_credentials.h`:

```c
#define WIFI_SSID   "your_network_ssid"
#define WIFI_PASS   "your_network_password"
```

### 2. (Optional) Adjust timezone

The default timezone is Spain (CET/CEST). To change it, edit `TIMEZONE` in `main/BLERouter.c`:

```c
#define TIMEZONE  "CET-1CEST,M3.5.0,M10.5.0/3"
```

Use any valid POSIX TZ string (e.g. `"UTC0"` for UTC, `"EST5EDT,M3.2.0,M11.1.0"` for US Eastern).

### 3. Build and flash

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Expected payload (Manufacturer Specific Data)

| Bytes | Field       | Type     | Unit      |
|-------|-------------|----------|-----------|
| 0–1   | Company ID  | uint16_t | —         |
| 2–3   | Temperature | int16_t  | °C × 100  |
| 4–5   | Humidity    | uint16_t | %RH × 100 |
| 6–7   | Pressure    | uint16_t | hPa × 10  |
