# AMAN-OS 🎮

A full-featured operating system for Arduino Nano (ATmega328P) with 6 games, multiple apps, and persistent storage - all controlled with a single button on a 16x2 I2C LCD display.

![AMAN-OS Demo](demo.gif)
*Demo coming soon!*

## ✨ Features

### 🎮 Games
- **Runner** - Endless runner with obstacles and coins
- **Snake** - Classic snake game with growing difficulty
- **Reaction** - Test your reaction time
- **Pong** - Single-player pong against AI
- **Memory** - Simon-like memory challenge
- **Flappy** - Flappy Bird clone with lives system

### 🛠️ Apps
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

## 🔧 Hardware Requirements

- **Arduino Nano** (ATmega328P)
- **16x2 I2C LCD Display** (I2C address: 0x27)
- **Push Button** (connected to pin D2)
- **Jumper wires**
- **Breadboard** (optional)

### Wiring Diagram
```
Arduino Nano → LCD (I2C)
- A4 (SDA) → SDA
- A5 (SCL) → SCL
- 5V → VCC
- GND → GND

Arduino Nano → Button
- D2 → Button → GND (internal pullup enabled)
```

## 📚 Libraries Required

Install these libraries via Arduino IDE Library Manager:
- `LiquidCrystal_I2C` by Frank de Brabander (or compatible)
- `EEPROM` (built-in with Arduino)

## 🚀 Installation

1. **Install Arduino IDE** (if not already installed)
2. **Install required libraries** (see above)
3. **Clone or download this repository**
```bash
   git clone https://github.com/toponerld-ops/Aman-OS.git
```
4. **Open `sketch_nov22b.ino` in Arduino IDE**
5. **Select your board**: Tools → Board → Arduino Nano
6. **Select your processor**: Tools → Processor → ATmega328P (Old Bootloader if needed)
7. **Select your port**: Tools → Port → (your Arduino's port)
8. **Upload** the sketch to your Arduino Nano

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
- **Runner**: Hold button to jump
- **Snake**: Click to change direction (up/down)
- **Reaction**: Click as fast as you can when "NOW!" appears
- **Pong**: Click to move paddle up/down
- **Memory**: Click (top) or Hold (bottom) to repeat sequence
- **Flappy**: Click to flap up, release to fall

## 🏗️ Technical Architecture

### State Machine
The OS uses a finite state machine with 17 states:
- Boot, Idle, Main Menu, Games Menu
- 6 Game states (Runner, Snake, Reaction, Pong, Memory, Flappy)
- 4 App states (Stopwatch, Calculator, Dice, Stats)
- Settings, Screensaver, Paused

### Memory Optimization
- **Buffered LCD Rendering**: Only updates changed characters to reduce I2C traffic
- **EEPROM Storage Layout**:
  - Bytes 0-11: High scores (2 bytes × 6 games)
  - Bytes 12-17: Games played counter (1 byte × 6)
  - Bytes 18-29: Total play time in seconds (2 bytes × 6)
  - Bytes 30-35: Difficulty settings (1 byte × 6)

### Performance
- **40 FPS frame limiter** ensures consistent gameplay
- **Non-blocking game loops** keep UI responsive
- **Optimized button debouncing** with cooldown timers
- **Differential updates** minimize LCD refresh overhead

## 🐛 Known Issues

- Minor bugs present (will be fixed in future updates)
- Some games may need difficulty balancing

## 🔮 Future Improvements

- [ ] Add more games
- [ ] Implement sound effects (buzzer support is in code)
- [ ] Battery level indicator
- [ ] More customization options
- [ ] Multiplayer support (2-button mode)
- [ ] Custom characters for better graphics

## 📸 Screenshots

*Coming soon - will add photos and videos of the OS in action!*

## 🤝 Contributing

Feel free to fork this project and submit pull requests! Some areas that need work:
- Bug fixes
- New games
- Performance optimizations
- Better graphics using custom LCD characters
- Documentation improvements

## 📜 License

This project is open source. Feel free to use and modify for your own projects!

## 🙏 Credits

Created by **toponerld-ops** (Aman)

Built for the Hack Club community! 🚢

---

**Hack Club Project** - Join us at [hackclub.com](https://hackclub.com)
