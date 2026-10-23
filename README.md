# SpartHill Smart Cane

A LiDAR-equipped cane that warns blind and low-vision users about dips, steps, and ledges ahead of them, with alerts delivered through their phone.

## The Problem

A traditional white cane only detects what it physically touches. A curb, a missing stair, or a drop-off can be found too late, when the user is already stepping into it. The SpartHill Smart Cane scans the ground about **6 ft ahead** so the user gets a warning before they reach the hazard.

## How It Works

1. A LiDAR distance sensor is mounted on the cane, angled down and forward so it measures the ground about 6 ft in front of the cane tip.
2. On flat ground the reading stays steady. The system records this reading as a **baseline** at startup and keeps updating it as the user walks.
3. When a reading comes back **longer** than the baseline, the ground is dropping away. The size of the difference tells us how serious the drop is.
4. The Arduino classifies the drop and sends an alert to the user's phone over Bluetooth. The phone then beeps, vibrates, or sounds an alarm.

```
┌──────────┐     ┌────────────────────┐     ┌───────────┐     ┌──────────────────────┐
│  LiDAR   │ ──► │  Arduino Mega 2560 │ ──► │ Bluetooth │ ──► │        Phone         │
│ (~6 ft)  │     │ baseline + classify│     │  module   │     │  beep/vibrate/alarm  │
└──────────┘     └────────────────────┘     └───────────┘     └──────────────────────┘
```

### Alert Levels

| Level | Example                       | Depth below baseline* | Phone alert   |
|-------|-------------------------------|-----------------------|---------------|
| 1     | Slight dip (pothole, crack)   | ~2–4 in               | 🔔 Beep        |
| 2     | Deep dip (curb, single step)  | ~4–10 in              | 📳 Vibration   |
| 3     | Ledge (staircase, drop-off)   | > 10 in, or no return | 🚨 Alarm sound |

\* Thresholds are starting estimates and will be tuned during testing.

### Why Alerts Come From the Phone

- Users already carry a phone, so no extra device is needed.
- Phones have strong vibration motors and loud speakers.
- The cane stays light and simple, with no buzzer or motor on the handle.
- Alert volume, vibration patterns, and accessibility settings can be adjusted in the app.

## Hardware

| Component            | Part                                     |
|----------------------|------------------------------------------|
| Microcontroller      | Arduino Mega 2560                        |
| Distance sensor      | LiDAR — *TBD (e.g. TF-Luna, VL53L1X)*    |
| Wireless link        | Bluetooth module — *TBD (e.g. HC-05, HM-10)* |
| Power                | Battery pack — *TBD*                     |
| Mount                | Cane bracket for sensor and electronics  |

## Software

The firmware is built with [PlatformIO](https://platformio.org/) using the Arduino framework.

### Getting Started

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE** extension (VS Code will recommend it when you open this folder), or install the CLI with `pip install platformio`.
2. Clone this repository and open it.
3. Plug in the Arduino Mega over USB.

```bash
pio run                 # build the firmware
pio run -t upload       # build and upload to the Arduino Mega
pio device monitor      # open the serial monitor (9600 baud)
```

### Project Structure

```
├── platformio.ini   # PlatformIO config (board: megaatmega2560)
├── src/             # Firmware source (main.cpp)
├── include/         # Header files
├── lib/             # Project libraries
└── test/            # Unit tests
```

## Status / Roadmap

- [x] PlatformIO project set up and upload to the Arduino Mega verified (blink test)
- [ ] Read distance data from the LiDAR sensor
- [ ] Baseline calibration and dip / step / ledge classification
- [ ] Send alerts over Bluetooth
- [ ] Phone app that plays the beep, vibration, and alarm
- [ ] Mount hardware on the cane
- [ ] Field testing and threshold tuning

## Safety Note

This is a hackathon prototype. It is meant to **supplement** standard white-cane technique, not replace it.

## Team

*Add team members here.*
