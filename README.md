# 🔑 Interactive Smart Keychain (ESP32-Based)

## 🔹 Short Introduction
Interactive Smart Keychain is an ESP32-based prototype that combines an OLED “RoboEyes” display with RGB LED feedback and a single-button input system.  
The keychain reacts to user taps and holds with different facial expressions, LED colors, and modes such as WiFi setup and an NTP-synced clock display.

This project focuses on building a responsive embedded interaction system using non-blocking logic, input debouncing, and clear state management.

---

## 🛠 Technologies Used
- ESP32
- Arduino IDE / Arduino Framework (C/C++)
- I2C (Wire)
- OLED Display (Adafruit SSD1306 + Adafruit GFX)
- FluxGarage RoboEyes library
- WiFi + WiFiManager (captive portal)
- NTP time sync (`time.h`)

---

## ✨ Features (What the User Can Do)
- View animated RoboEyes expressions on an OLED display
- Trigger different interactions using tap patterns (single/multiple taps)
- Use a **hold gesture** for a “petting” feedback mode with blinking LED
- Enter **WiFi Setup mode** using a captive portal (no hardcoded credentials)
- Display a **real-time clock screen** synced via NTP (requires WiFi)
- Enjoy automatic idle animations that change over time
- Receive RGB LED feedback for each mode/interaction

---

## 🕹 Controls / Interaction Guide (Button Input)
Button is configured as **pressed = HIGH**.

### Tap Actions
- **1 Tap**: Wink expression + yellow LED feedback  
- **2 Taps**: Shy/love reaction + red/pink LED feedback  
- **3 Taps**: “Cry” feedback + red LED (error/negative reaction)  
- **4 Taps**: Enter **WiFi Setup Portal** (AP: `RoboEyes-Setup`)  
- **5 Taps**: Show **Clock Mode** (NTP time display)

### Hold Action
- **Hold (≥ 600ms)**: “Pet” feedback mode  
  - Yellow blinking LED + happy expression while holding

### WiFi Setup Mode
- **1 Tap** inside WiFi setup: Exit/Cancel and return to idle

---

## 🏗 How I Built It (Development Process)
1. Set up OLED display rendering using Adafruit SSD1306 and RoboEyes  
2. Implemented RGB LED control with preset colors and blink timing  
3. Built a reliable button input system:
   - Debounce handling
   - Tap counting with timeout
   - Hold detection threshold
4. Designed a state machine architecture using modes:
   - `IDLE`, `FEEDBACK`, `WIFI_PORTAL`, `CLOCK`
5. Implemented non-blocking WiFiManager portal:
   - Portal runs while button input is still responsive
   - Manual timeout handling to safely exit to idle
6. Added NTP time sync and a clock display mode with refresh interval
7. Refined idle behavior:
   - Random idle expression changes every few seconds
   - LED color matched to expression/mood

---

## 📚 What I Learned
- Building responsive embedded interaction systems using finite state machines
- Designing robust button gesture logic (debounce, multi-tap, hold)
- Avoiding blocking code so the UI remains responsive
- Integrating OLED animations with hardware feedback (RGB LED)
- Implementing WiFi captive portals for better user setup experience
- Syncing and rendering real-time clock data with NTP

---

## 🚀 What Could Be Improved
- Add audio feedback using DFPlayer Mini (sound per expression/mode)
- Save user preferences (e.g., brightness, last mode) using NVS/EEPROM
- Add more gestures (double-hold, long-hold, tap+hold combos)
- Improve power management (sleep mode, battery monitoring)
- Add smoother animation transitions and more expression variety

---

## ▶ How to Run the Project
1. Install required libraries:
   - Adafruit GFX Library
   - Adafruit SSD1306
   - FluxGarage RoboEyes
   - WiFiManager
2. Open the `.ino` file in Arduino IDE
3. Select your ESP32 board and correct COM port
4. Upload to ESP32
5. Optional:
   - Connect to WiFi via **4 taps** → join `RoboEyes-Setup` → open `192.168.4.1`
6. Use **5 taps** to display clock mode (WiFi required)

---
## 🔌 Hardware Wiring
<img width="3000" height="2758" alt="Image" src="https://github.com/user-attachments/assets/436b2703-03d6-4617-bad4-5b4e88bf520a" />

## 🎥 Demo Video
https://github.com/user-attachments/assets/39e8fa40-d38d-435a-b0b3-ef0333c10ae5
