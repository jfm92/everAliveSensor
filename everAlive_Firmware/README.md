# everAlive Firmware

Firmware for the **everAlive** environmental sensor node. The device reads temperature, humidity, and barometric pressure from a BME680 sensor and broadcasts the data as a BLE advertisement packet. After each advertising burst the MCU enters deep sleep to maximise battery life.

## Hardware

| Item | Details |
|------|---------|
| MCU | ESP32-C6 |
| Sensor | Bosch BME680 (I²C) |
| SDA | GPIO 7 |
| SCL | GPIO 6 |
| I²C address | `0x77` |
| I²C speed | 400 kHz (Fast Mode) |

## Software stack

| Layer | Version |
|-------|---------|
| ESP-IDF | 5.5.4 |
| NimBLE (BLE stack) | bundled with IDF |
| BME68x SensorAPI | v4.4.8 (git submodule) |

## Repository setup

The Bosch BME68x driver lives in `components/bme680x` as a **git submodule**. You must initialise it before building.

```bash
# Clone including submodules
git clone --recurse-submodules <repo-url>

# -- or, if you already cloned without submodules --
git submodule update --init --recursive
```

## Prerequisites

1. **ESP-IDF 5.5.4** installed and sourced:

   ```bash
   . $HOME/esp/esp-idf/export.sh   # adjust path to your install
   ```

2. **USB serial driver** for ESP32-C6 (CP210x or built-in USB-CDC depending on your board).

## Build & flash

```bash
# Configure target (only needed once)
idf.py set-target esp32c6

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with the serial port assigned to your board.

## Configuration

Key parameters are defined in `main/common.h`:

| Macro | Default | Description |
|-------|---------|-------------|
| `DEVICE_NAME` | `"EnvSensor"` | BLE advertised name |
| `ADVERTISE_DURATION_MS` | `20` | How long (ms) to advertise per cycle |
| `SLEEP_DURATION_S` | `300` | Deep-sleep duration between cycles (s) |

To enable debug logging, comment out `#define SILENT_MODE` at the top of `main/main.c`.

## BLE advertisement payload

The sensor data is packed into the manufacturer-specific field of the BLE advertisement using the following structure (little-endian, 6 bytes):

| Field | Type | Scale | Unit |
|-------|------|-------|------|
| `temperature` | `int16_t` | ÷ 100 | °C |
| `humidity` | `uint16_t` | ÷ 100 | %RH |
| `pressure` | `uint16_t` | ÷ 10 | hPa |

The receiver must use the same `sensor_payload_t` definition (see `main/common.h`).

## Project structure

```
everAlive_Firmware/
├── main/
│   ├── main.c          # Entry point, NimBLE host task
│   ├── gap.c / gap.h   # BLE GAP advertising
│   ├── common.h        # Shared types and macros
│   └── sensor/
│       ├── bme680.c    # BME680 I²C driver wrapper
│       └── bme680.h
├── components/
│   └── bme680x/        # Bosch BME68x SensorAPI (git submodule)
├── CMakeLists.txt
└── sdkconfig.defaults  # Minimal build defaults (BLE enabled, logs off)
```

## License

See [LICENSE](../LICENSE) in the project root.
