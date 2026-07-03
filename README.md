# 📡 RogueOS: Tactical RF Fox-Hunting & Forensic Firmware

![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue?style=for-the-badge&logo=espressif)
![Hardware](https://img.shields.io/badge/Hardware-M5Cardputer%20ADV-red?style=for-the-badge)
![Framework](https://img.shields.io/badge/Framework-Arduino%20%2F%20PlatformIO-green?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)

**RogueOS** turns the **M5Stack Cardputer ADV** into an autonomous, hand-held RF direction-finding (fox-hunting) and Wi-Fi forensic profiling platform. 

By fusing high-speed 2.4GHz channel polling with real-time spatial orientation data from the onboard **BMI270 6-Axis IMU**, RogueOS plots relative signal strength onto a live polar radar screen. Spin 360 degrees to triangulate rogue access points, hidden transmitters, and client devices without needing bulky SDR hardware.

---

## 🔥 Key Features

* **🧭 IMU-Driven Spatial Radar:** Integrates dynamic delta-time ($dt$) Z-axis gyroscope tracking via `M5Unified`. As you physically rotate, the firmware maps RSSI (Received Signal Strength Indicator) against your relative heading to build a visual "heat signature" of the transmitting source.
* **🎯 Single-Channel High-Speed Locking:** Select a target and lock the ESP32 radio chip to its specific operating channel. This drops poll times down to ~150ms for hyper-responsive spatial tracking.
* **☢️ Geiger-Counter Audio Feedback:** Leverages the Cardputer ADV's upgraded **ES8311 audio codec** and speaker/3.5mm jack. Tone pitch dynamically scales from 400 Hz to 2000 Hz as you rotate toward the signal peak—allow you to track targets eyes-free.
* **🕵️ Forensic Detail Mode:** Press `TAB` while locked onto a target to reveal deep RF metadata:
  * **OUI Vendor Identification:** Automatically resolves MAC addresses to hardware manufacturers (Apple, Google, Espressif, Samsung, Intel, Cisco, etc.).
  * **Security Classification:** Detects open networks, WEP, WPA, WPA2, WPA3, and transitional security modes.
  * **Estimated SNR:** Calculates approximate Signal-to-Noise Ratio against the background ambient noise floor.
* **⚡ Flicker-Free 60 FPS UI:** Built entirely on double-buffered `M5Canvas` memory sprites to prevent visual tearing and LCD ghosting during rapid sweeps.

---

## 🛠️ Hardware Requirements

* **M5Stack Cardputer ADV** (ESP32-S3 Stamp-S3A module)
  * *Note: Requires the ADV hardware revision for BMI270 IMU and ES8311 Audio support.*
* *(Optional)* **M5Stack LoRa / GNSS Cap** via the EXT 2.54-14P top expansion bus for high-gain directional antennas or GPS waypoint tagging.

---

## 🎮 Tactical Keyboard Controls

RogueOS uses the Cardputer's 56-key mechanical keyboard for rapid field navigation:

| Key / Input | Action | Description |
| :--- | :--- | :--- |
| **`SPACE`** | **Scan Airspace** | Triggers an asynchronous background scan across all 13 2.4GHz Wi-Fi channels. |
| **`,`** / **`.`** | **Cycle Targets** | Scrolls up and down through the list of discovered access points and BSSIDs. |
| **`ENTER`** | **Target Lock** | Locks the radio to the selected network's channel and initiates live radar mapping. |
| **`TAB`** | **Toggle Page** | Switches the right-hand dashboard between **Radar Telemetry** and **Forensic Detail** mode. |
| **`BACKSPACE`** | **Abort / Unlock** | Drops target lock, clears historical point cloud data, and returns to target selection. |

---

## 🖥️ User Interface Overview

### Page 1: Live Radar Telemetry (`viewMode = 0`)
* **Left Column:** 360° Polar Reticle. White line indicates current IMU heading. Green dots represent historical RSSI samples fading over time. Red dot locks onto the absolute peak signal vector.
* **Right Column:** Active SSID, operating channel, real-time RSSI (dBm), current heading (°), and locked peak vector bearing (°).

### Page 2: Forensic Target Data (`viewMode = 1`)
* **Left Column:** Continuous live radar sweep.
* **Right Column:** Deep inspection dashboard displaying hardware MAC address (BSSID), resolved hardware vendor, encryption standard, and estimated SNR.

---

## 📦 Installation & Setup

### Option 1: PlatformIO (Recommended)
1. Clone this repository:
   ```bash
   git clone [https://github.com/yourusername/RogueOS.git](https://github.com/yourusername/RogueOS.git)
   cd RogueOS