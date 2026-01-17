# AMAN-OS 🎮

![Arduino](https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=Arduino&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)
![Version](https://img.shields.io/badge/Version-1.0-brightgreen?style=for-the-badge)
![Build](https://img.shields.io/badge/Build-Passing-success?style=for-the-badge)
![Games](https://img.shields.io/badge/Games-6-blue?style=for-the-badge)
![Apps](https://img.shields.io/badge/Apps-5-orange?style=for-the-badge)
![Development](https://img.shields.io/badge/Dev_Time-77.4hrs-red?style=for-the-badge)# AMAN-OS 🎮
A full-featured operating system for Arduino Nano (ATmega328P) with 6 games, multiple apps, wireless weather monitoring, and persistent storage - all controlled with a single button on a 16x2 I2C LCD display.

## 📺 Demo Videos

### AMAN-OS Full System Demo
[![AMAN-OS Demo](https://img.youtube.com/vi/J0eP7VAG8SE/maxresdefault.jpg)](https://youtu.be/J0eP7VAG8SE?si=_ESCnH3g4pu2qovO)

**[▶️ Watch Full Demo on YouTube](https://youtu.be/J0eP7VAG8SE?si=_ESCnH3g4pu2qovO)**

### Weather Station Extension Demo
[![Weather Station Demo](https://img.youtube.com/vi/XZgUcti3Fz8/maxresdefault.jpg)](https://youtu.be/XZgUcti3Fz8?si=h-nKdIMIbGmG5xgP)

**[▶️ Watch Weather Station Demo on YouTube](https://youtu.be/XZgUcti3Fz8?si=h-nKdIMIbGmG5xgP)**

---

## ✨ Features

### 🎮 Games
- **Runner** - Endless runner with obstacles and coins
- **Snake** - Classic snake game with growing difficulty
- **Reaction** - Test your reaction time
- **Pong** - Single-player pong against AI
- **Memory** - Simon-like memory challenge
- **Flappy** - Flappy Bird clone with lives system

### 🛠️ Apps
- **Weather Monitor** - Wireless DHT11 sensor data via Bluetooth
- **Stopwatch** - Precision timing with ms accuracy
- **Calculator** - Basic arithmetic operations
- **Dice Roller** - Configurable dice (D4 to D20)
- **Stats Viewer** - Track game statistics
- **Settings** - Difficulty configuration per game

### 💾 System Features
- Persistent high scores stored in EEPROM
- Total play time tracking per game
- Games played counter
- Difficulty settings (Easy/Normal/Hard) per game
- Slot machine-style boot animation
- Screensaver with bouncing animation
- Pause menu (hold button during gameplay)
- Buffered LCD rendering for optimized performance
- 40 FPS frame limiter for smooth gameplay
- **Dual-Arduino wireless communication** for weather monitoring

---

## 🔧 Hardware Requirements

### Main System (AMAN-OS)
- Arduino Nano (ATmega328P)
- 16x2 I2C LCD Display (I2C address: 0x27)
- Push Button (connected to pin D2)
- HC-05 Bluetooth Module (optional - for weather station)
- Jumper wires
- Breadboard

### Weather Station (Optional Extension)
- Arduino Uno
- DHT11 Temperature & Humidity Sensor
- HC-05 Bluetooth Module (slave mode)
- Jumper wires
- Breadboard
- 2× 1N4148 Diodes (for voltage level protection)

---

## 🔌 Wiring Diagrams

### Main System (AMAN-OS - Arduino Nano)
```
Arduino Nano → LCD (I2C)
- A4 (SDA) → SDA
- A5 (SCL) → SCL
- 5V → VCC
- GND → GND

Arduino Nano → Button
- D2 → Button → GND (internal pullup enabled)

Arduino Nano → HC-05 Bluetooth (Optional)
- D10 (RX) → HC-05 TX
- D11 (TX) → HC-05 RX (through 2× diodes)
- 5V → VCC
- GND → GND
```

### Weather Station (Optional - Arduino Uno)
```
Arduino Uno → DHT11
- D2 → DHT11 DATA
- 5V → DHT11 VCC
- GND → DHT11 GND
- 10kΩ resistor between DATA and VCC (pull-up)

Arduino Uno → HC-05 Bluetooth
- D10 (RX) → HC-05 TX
- D11 (TX) → HC-05 RX (through 2× diodes)
- 5V → VCC
- GND → GND
```

**Voltage Level Protection:**
```
Arduino TX (5V) → Diode → Diode → HC-05 RX (3.3V)
                   1N4148   1N4148
```

---

## 📚 Libraries Required

Install these libraries via Arduino IDE Library Manager:

### For Main System:
- `LiquidCrystal_I2C` by Frank de Brabander (or compatible)
- `EEPROM` (built-in with Arduino)
- `SoftwareSerial` (built-in with Arduino)

### For Weather Station:
- `DHT sensor library` by Adafruit
- `SoftwareSerial` (built-in with Arduino)

---

## 🚀 Installation

### Main System Setup
1. Install Arduino IDE (if not already installed)
2. Install required libraries (see above)
3. Clone or download this repository:
   ```bash
   git clone https://github.com/toponerld-ops/Aman-OS.git
   ```
4. Open `sketch_nov22b.ino` in Arduino IDE
5. Select your board: **Tools → Board → Arduino Nano**
6. Select your processor: **Tools → Processor → ATmega328P (Old Bootloader if needed)**
7. Select your port: **Tools → Port → (your Arduino's port)**
8. Upload the sketch to your Arduino Nano

### Weather Station Setup (Optional)
1. Open `weather_station_slave.ino` in Arduino IDE
2. Select **Tools → Board → Arduino Uno**
3. Upload to your Arduino Uno
4. Configure HC-05 modules:
   - Master (on Nano): `AT+ROLE=1`, `AT+CMODE=0`
   - Slave (on Uno): `AT+ROLE=0`
5. Pair the Bluetooth modules
6. Power on both systems - weather data should appear!

---

## 🎮 How to Use

### Single Button Controls
- **Short Press (Click)** - Navigate menus / Toggle options
- **Long Press (Hold 1.5s)** - Select / Confirm
- **Very Long Hold (2s in-game)** - Pause menu

### Navigation
1. **Idle Screen** → Press button to enter main menu
2. **Main Menu** → Click to cycle through options, hold to select
3. **Games Menu** → Click to browse games, hold to start
4. **In-Game** → Controls vary per game (flap, jump, move, etc.)
5. **Pause** → Hold button for 2 seconds during gameplay

### Game Controls
- **Runner:** Hold button to jump
- **Snake:** Click to change direction (up/down)
- **Reaction:** Click as fast as you can when "NOW!" appears
- **Pong:** Click to move paddle up/down
- **Memory:** Click (top) or Hold (bottom) to repeat sequence
- **Flappy:** Click to flap up, release to fall

---

## 🏗️ Technical Architecture

### State Machine
The OS uses a finite state machine with **18 states**:
- Boot, Idle, Main Menu, Games Menu
- 6 Game states (Runner, Snake, Reaction, Pong, Memory, Flappy)
- 5 App states (Weather, Stopwatch, Calculator, Dice, Stats)
- Settings, Screensaver, Paused

### Memory Optimization
**Buffered LCD Rendering:** Only updates changed characters to reduce I2C traffic

**EEPROM Storage Layout:**
- Bytes 0-11: High scores (2 bytes × 6 games)
- Bytes 12-17: Games played counter (1 byte × 6)
- Bytes 18-29: Total play time in seconds (2 bytes × 6)
- Bytes 30-35: Difficulty settings (1 byte × 6)

### Performance
- 40 FPS frame limiter ensures consistent gameplay
- Non-blocking game loops keep UI responsive
- Optimized button debouncing with cooldown timers
- Differential updates minimize LCD refresh overhead
- Binary data transmission for efficient Bluetooth communication

---

## 🌡️ Weather Station Protocol

The weather monitoring system uses a master-slave architecture:

### Communication Protocol
1. Master sends: `"REQ"` (3 bytes ASCII)
2. Slave responds: 8 bytes binary data
   - Bytes 0-3: Temperature (float, 4 bytes)
   - Bytes 4-7: Humidity (float, 4 bytes)

### Features
- Automatic reconnection on timeout (15 seconds)
- Error handling for sensor failures
- Real-time data updates
- Connection status indicator
- Non-blocking wireless communication

---

## 🐛 Known Issues
- Minor bugs present (will be fixed in future updates)
- Some games may need difficulty balancing
- Weather station requires manual Bluetooth pairing

---

## 🔮 Future Improvements
- [ ] Add more games
- [ ] Implement sound effects (buzzer support)
- [ ] Battery level indicator
- [ ] More customization options
- [ ] Multiplayer support (2-button mode)
- [ ] Custom characters for better graphics
- [ ] Soil moisture sensor for plant monitoring
- [ ] OLED display support

---

## 🤝 Contributing
Feel free to fork this project and submit pull requests! Some areas that need work:
- Bug fixes
- New games
- Performance optimizations
- Better graphics using custom LCD characters
- Documentation improvements
- Additional sensors for weather station

---

## 📜 License
This project is open source. Feel free to use and modify for your own projects!

---

## 🙏 Credits
**Created by:** toponerld-ops (Aman)

**Development Time:** 77.4 hours across 12 days

Built for the **Hack Club** community! 🚢

[**Join us at hackclub.com**](https://hackclub.com)

---

## 📊 Project Stats
- **Lines of Code:** ~2,500+
- **Total States:** 18
- **Games:** 6
- **Apps:** 5
- **Hardware Modules:** 2 Arduinos + 4 sensors/modules
- **Development Time:** 77.4 hours
- **Troubleshooting Time:** 25 hours

---

## 📺 Watch the Demos!

### Full System Demonstration
[![AMAN-OS Demo](https://img.youtube.com/vi/J0eP7VAG8SE/maxresdefault.jpg)](https://youtu.be/J0eP7VAG8SE?si=_ESCnH3g4pu2qovO)

**[▶️ Click to Watch on YouTube](https://youtu.be/J0eP7VAG8SE?si=_ESCnH3g4pu2qovO)**

### Weather Station in Action
[![Weather Station Demo](https://img.youtube.com/vi/XZgUcti3Fz8/maxresdefault.jpg)](https://youtu.be/XZgUcti3Fz8?si=h-nKdIMIbGmG5xgP)

**[▶️ Click to Watch on YouTube](https://youtu.be/XZgUcti3Fz8?si=h-nKdIMIbGmG5xgP)**
