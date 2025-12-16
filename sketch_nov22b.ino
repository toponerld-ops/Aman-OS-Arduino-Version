/* 
  AMAN-OS (full) - Single-button 16x2 I2C LCD OS for Arduino Nano (ATmega328P)
  - 6 games: Runner, Snake, Reaction, Pong, Memory, Flappy
  - Highscores, gamesPlayed, totalPlayTime saved in EEPROM
  - Difficulty per game saved in EEPROM
  - Splash screen, settings, high-score screen, pause menu
  - Buffered LCD renderer (diff updates)
  - Non-blocking main loop with 40 FPS frame limiter
  Single button: D2 -> to GND
  LCD: LiquidCrystal_I2C at 0x27,16,2
*/

#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ---------- HARDWARE CONFIG ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define PIN_BUTTON 2
// Optional buzzer: add pin and implement tone() calls if you later want sound
#define BUZZER_PIN 255 // 255 -> disabled

// Battery (disabled by default - user didn't provide pin)
#define BATTERY_ENABLED false
#define BATTERY_PIN A0

// ---------- TIMING / FRAME LIMITER ----------
const uint8_t FPS = 40;               // 40 FPS as requested
const uint16_t FRAME_TIME = 1000 / FPS;
unsigned long lastFrame = 0;
bool frameReady() {
  unsigned long now = millis();
  if (now - lastFrame >= FRAME_TIME) {
    lastFrame = now;
    return true;
  }
  return false;
}

// ---------- BUTTON (single-button semantics) ----------
#define HOLD_TIME 1500
#define PAUSE_HOLD_TIME 2000
#define CLICK_COOLDOWN 400

// buttonAction meanings:
// 0 = none, 1 = click, 2 = hold (select/confirm)
static byte buttonAction = 0;
static bool btnWasPressed = false;
static unsigned long btnDownTime = 0;
static unsigned long lastClickTime = 0;
static bool holdingForPause = false;

// ---------- STATES ----------
#define STATE_BOOT 0
#define STATE_IDLE 1
#define STATE_MAIN_MENU 2
#define STATE_GAMES_MENU 3
#define STATE_RUNNER 4
#define STATE_SNAKE 5
#define STATE_REACTION 6
#define STATE_PONG 7
#define STATE_MEMORY 8
#define STATE_FLAPPY 9
#define STATE_SETTINGS 10
#define STATE_STOPWATCH 11
#define STATE_CALCULATOR 12
#define STATE_DICE 13
#define STATE_SCREENSAVER 14
#define STATE_STATS 15
#define STATE_PAUSED 16

static byte gameState = STATE_BOOT;
static byte previousGame = STATE_RUNNER;

// ---------- GAMES / UI CONSTANTS ----------
#define TERRAIN_WIDTH 16
#define HERO_HORIZONTAL_POSITION 1

const char* mainMenuItems[] = {"Games", "Stopwatch", "Calculator", "Dice", "Stats", "Settings"};
const char* gameNames[] = {"Runner", "Snake", "Reaction", "Pong", "Memory", "Flappy"};

// ---------- EEPROM STORAGE LAYOUT ----------
/*
0..11   : highScores[0..5]   (2 bytes each)  -> 12 bytes
12..17  : gamesPlayed[0..5]  (1 byte each)  -> 6 bytes
18..29  : totalPlayTime[0..5] (2 bytes each) -> 12 bytes (seconds)
30..35  : difficulty[0..5]    (1 byte each)  -> 6 bytes
36.. : reserved
*/
const int EEPROM_ADDR_HIGHSCORES = 0;
const int EEPROM_ADDR_GAMESPLAYED = 12;
const int EEPROM_ADDR_TOTALPLAYTIME = 18;
const int EEPROM_ADDR_DIFFICULTY = 30;

// ---------- STATS ----------
static uint16_t highScores[6];
static uint16_t totalPlayTime[6]; // seconds (2 bytes each)
static uint8_t gamesPlayed[6];
static uint8_t difficultyPerGame[6]; // 0=Easy,1=Normal,2=Hard

// ---------- Global game vars ----------
static bool gameIsActive = false;
static unsigned long gameStartTime = 0;

// Buffered LCD renderer (stores last written chars)
char lcdBuf[2][17]; // last frame, plus terminating \0

void lcdInitBuffer() {
  for (int r = 0; r < 2; ++r) {
    for (int c = 0; c < 16; ++c) lcdBuf[r][c] = '\xff'; // sentinel different from ' '
    lcdBuf[r][16] = '\0';
  }
}

void lcdWriteBuffered(int col, int row, const char* text) {
  // writes text starting at col,row but only updates changed chars
  for (int i = 0; text[i] && col + i < 16; ++i) {
    char ch = text[i];
    if (lcdBuf[row][col + i] != ch) {
      lcd.setCursor(col + i, row);
      lcd.print(ch);
      lcdBuf[row][col + i] = ch;
    }
  }
}

// Overload to write single char
void lcdWriteBufferedChar(int col, int row, char ch) {
  if (col < 0 || col >= 16 || row < 0 || row >= 2) return;
  if (lcdBuf[row][col] != ch) {
    lcd.setCursor(col, row);
    lcd.print(ch);
    lcdBuf[row][col] = ch;
  }
}

void lcdClearBuffered() {
  for (int r = 0; r < 2; ++r) for (int c = 0; c < 16; ++c) lcdBuf[r][c] = '\0';
  lcd.clear();
}

// ---------- EEPROM helpers ----------
void loadStatsFromEEPROM() {
  int addr = EEPROM_ADDR_HIGHSCORES;
  for (int i = 0; i < 6; ++i) {
    uint16_t v = 0;
    v = EEPROM.read(addr) | (EEPROM.read(addr + 1) << 8);
    highScores[i] = v;
    addr += 2;
  }
  addr = EEPROM_ADDR_GAMESPLAYED;
  for (int i = 0; i < 6; ++i) gamesPlayed[i] = EEPROM.read(addr++);

  addr = EEPROM_ADDR_TOTALPLAYTIME;
  for (int i = 0; i < 6; ++i) {
    uint16_t v = EEPROM.read(addr) | (EEPROM.read(addr + 1) << 8);
    totalPlayTime[i] = v;
    addr += 2;
  }

  addr = EEPROM_ADDR_DIFFICULTY;
  for (int i = 0; i < 6; ++i) {
    uint8_t d = EEPROM.read(addr++);
    if (d > 2) d = 1;
    difficultyPerGame[i] = d;
  }
}

void saveHighScoreToEEPROM(byte game) {
  int addr = EEPROM_ADDR_HIGHSCORES + game * 2;
  uint16_t v = highScores[game];
  EEPROM.update(addr, v & 0xFF);
  EEPROM.update(addr + 1, (v >> 8) & 0xFF);
}

void saveGamesPlayedToEEPROM(byte game) {
  int addr = EEPROM_ADDR_GAMESPLAYED + game;
  EEPROM.update(addr, gamesPlayed[game]);
}

void saveTotalPlayTimeToEEPROM(byte game) {
  int addr = EEPROM_ADDR_TOTALPLAYTIME + game * 2;
  uint16_t v = totalPlayTime[game];
  EEPROM.update(addr, v & 0xFF);
  EEPROM.update(addr + 1, (v >> 8) & 0xFF);
}

void saveDifficultyToEEPROM(byte game) {
  int addr = EEPROM_ADDR_DIFFICULTY + game;
  EEPROM.update(addr, difficultyPerGame[game]);
}

void saveAllStats() {
  for (byte i = 0; i < 6; ++i) {
    saveHighScoreToEEPROM(i);
    saveGamesPlayedToEEPROM(i);
    saveTotalPlayTimeToEEPROM(i);
    saveDifficultyToEEPROM(i);
  }
}

// ---------- Button update (non-blocking; sets buttonAction) ----------
void updateButton() {
  buttonAction = 0;
  bool pressed = (digitalRead(PIN_BUTTON) == LOW);
  unsigned long now = millis();

  if (pressed && !btnWasPressed) {
    btnDownTime = now;
    btnWasPressed = true;
    holdingForPause = false;
  }
  else if (pressed && btnWasPressed) {
    unsigned long held = now - btnDownTime;
    if (held >= PAUSE_HOLD_TIME && gameIsActive) {
      if (!holdingForPause) {
        holdingForPause = true;
        previousGame = gameState;
        gameState = STATE_PAUSED;
        // show pause menu immediately
        lcdClearBuffered();
        lcd.setCursor(3, 0); lcd.print("PAUSED");
        lcd.setCursor(0, 1); lcd.print(">Resume   Quit");
      }
      btnWasPressed = false; // consume
    }
    else if (held >= HOLD_TIME && !gameIsActive && gameState != STATE_PAUSED) {
      // long hold in menus = select
      buttonAction = 2;
      btnWasPressed = false;
    }
  }
  else if (!pressed && btnWasPressed) {
    btnWasPressed = false;
    holdingForPause = false;
    unsigned long held = now - btnDownTime;
    if (held < HOLD_TIME && now - lastClickTime > CLICK_COOLDOWN) {
      buttonAction = 1; // click
      lastClickTime = now;
    }
  }
}

// ---------- Splash / Boot (non-blocking-ish - short blocking animations okay) ----------
void slotMachineBoot() {
  // short blocking animation - one-time at boot
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("AMAN OS");
  delay(600);

  // slot-like reveal
  const char* target = "AMAN OS";
  char display[8] = "       ";
  for (int i = 0; i < 7; ++i) {
    if (target[i] == ' ') { display[i] = ' '; continue; }
    for (int spin = 0; spin < 10; ++spin) {
      display[i] = 'A' + random(26);
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print(display);
      delay(30 + spin * 6);
    }
    display[i] = target[i];
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print(display);
    delay(120);
  }
  // blink backlight if supported by library (no-op otherwise)
  for (int k = 0; k < 2; ++k) {
    // many LiquidCrystal_I2C implementations have noBacklight/backlight
    // We try, but guard with no side effects if not present.
#ifdef LiquidCrystal_I2C_h
    // nothing portable; keep short pause
#endif
    delay(80);
  }

  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Ready!");
  delay(300);
  lcd.clear();
}

// ---------- Idle animations ----------
static unsigned long idleLastActivity = 0;
void drawIdle() {
  static bool initialized = false;
  static bool blinkState = false;
  static unsigned long lastBlink = 0;

  if (!initialized) {
    lcdClearBuffered();
    lcd.setCursor(4, 0);
    lcd.print("AMAN OS");
    initialized = true;
    idleLastActivity = millis();
  }

  if (millis() - lastBlink > 500) {
    blinkState = !blinkState;
    lastBlink = millis();
    // small fight in corner
    lcdWriteBuffered(13, 1, blinkState ? "><" : "  ");
  }

  // If any button action comes, go to menu
  if (buttonAction == 1 || buttonAction == 2) {
    initialized = false;
    idleLastActivity = millis();
    gameState = STATE_MAIN_MENU;
    lcdClearBuffered();
    return;
  }

  // Screensaver after idle timeout (1 minute by default)
  const unsigned long IDLE_TIMEOUT = 60000;
  if (millis() - idleLastActivity > IDLE_TIMEOUT) {
    gameState = STATE_SCREENSAVER;
    idleLastActivity = millis();
    lcdClearBuffered();
  }
}

// ---------- Main menu ----------
byte mainMenuSelection = 0;
void drawMainMenu() {
  lcdClearBuffered();
  char buf[17];
  snprintf(buf, sizeof(buf), ">%s", mainMenuItems[mainMenuSelection]);
  lcdWriteBuffered(0, 0, buf);
  lcdWriteBuffered(0, 1, "Press to select ");
}

void updateMainMenu() {
  if (buttonAction == 1) {
    mainMenuSelection = (mainMenuSelection + 1) % 6;
    drawMainMenu();
  } else if (buttonAction == 2) {
    // hold to enter
    lcdClearBuffered();
    switch (mainMenuSelection) {
      case 0: gameState = STATE_GAMES_MENU; break;
      case 1: gameState = STATE_STOPWATCH; break;
      case 2: gameState = STATE_CALCULATOR; break;
      case 3: gameState = STATE_DICE; break;
      case 4: gameState = STATE_STATS; break;
      case 5: gameState = STATE_SETTINGS; break;
    }
    // immediate draw
    if (gameState == STATE_GAMES_MENU) {
      // draw games menu next frame
    }
  }
}

// ---------- Games Menu ----------
byte gamesMenuSelection = 0;
void drawGamesMenu() {
  lcdClearBuffered();
  char buf[17];
  snprintf(buf, sizeof(buf), ">%s", gameNames[gamesMenuSelection]);
  lcdWriteBuffered(0, 0, buf);
  char hi[17];
  snprintf(hi, sizeof(hi), "Hi:%u         ", highScores[gamesMenuSelection]);
  lcdWriteBuffered(0, 1, hi);
}

void updateGamesMenu() {
  if (buttonAction == 1) {
    gamesMenuSelection = (gamesMenuSelection + 1) % 6;
    drawGamesMenu();
  } else if (buttonAction == 2) {
    // start selected game
    switch (gamesMenuSelection) {
      case 0: gameState = STATE_RUNNER; break;
      case 1: gameState = STATE_SNAKE; break;
      case 2: gameState = STATE_REACTION; break;
      case 3: gameState = STATE_PONG; break;
      case 4: gameState = STATE_MEMORY; break;
      case 5: gameState = STATE_FLAPPY; break;
    }
    lcdClearBuffered();
  }
}

// ---------- Pause Menu ----------
byte pauseSelection = 0;
unsigned long pauseHoldStart = 0;
void updatePause() {
  if (buttonAction == 1) {
    pauseSelection = (pauseSelection + 1) % 2;
    pauseHoldStart = millis();
    if (pauseSelection == 0) lcd.setCursor(0, 1), lcd.print(">Resume   Quit  ");
    else lcd.setCursor(0, 1), lcd.print(" Resume  >Quit  ");
  }

  if (pauseHoldStart > 0) {
    unsigned long elapsed = millis() - pauseHoldStart;
    int progress = (elapsed * 9) / HOLD_TIME;
    if (progress > 9) progress = 9;
    lcd.setCursor(0, 0);
    if (pauseSelection == 0) lcd.print("RESUME?");
    else lcd.print("QUIT?");
    for (int i = 0; i < 9; ++i) lcd.print(i < progress ? '\xFF' : ' ');
    if (elapsed >= HOLD_TIME) {
      if (pauseSelection == 0) {
        gameState = previousGame;
        lcdClearBuffered();
      } else {
        gameIsActive = false;
        gameState = STATE_IDLE;
        lcdClearBuffered();
      }
      pauseHoldStart = 0;
      return;
    }
  } else {
    lcd.setCursor(0, 0);
    lcd.print("    PAUSED      ");
  }

  if (buttonAction == 2) {
    // quick select
    if (pauseSelection == 0) {
      gameState = previousGame;
      lcdClearBuffered();
    } else {
      gameIsActive = false;
      gameState = STATE_IDLE;
      lcdClearBuffered();
    }
    pauseHoldStart = 0;
  }
}

// ---------- RUNNER (improved, non-blocking) ----------
static char terrainUpper[TERRAIN_WIDTH + 1];
static char terrainLower[TERRAIN_WIDTH + 1];

void initRunnerGraphics() {
  for (int i = 0; i < TERRAIN_WIDTH; ++i) {
    terrainUpper[i] = ' ';
    terrainLower[i] = ' ';
  }
  terrainUpper[TERRAIN_WIDTH] = '\0';
  terrainLower[TERRAIN_WIDTH] = '\0';
}

void advanceTerrain(char* terrain, byte newTerrain) {
  // shift left (index 0 is leftmost), we model scrolling right-to-left visually
  for (int i = 0; i < TERRAIN_WIDTH - 1; ++i) terrain[i] = terrain[i + 1];
  // place new terrain at far right
  terrain[TERRAIN_WIDTH - 1] = (char)newTerrain;
}

void runnerGame() {
  static byte heroPos = 1; // 1..12 states for animation
  static byte newTerrainType = 0;
  static byte newTerrainDuration = 1;
  static unsigned long lastRunnerFrame = 0;
  static bool showTitle = true;
  static bool titleBlink = false;
  static unsigned long lastTitleBlink = 0;
  static unsigned int coins = 0;
  static bool showScore = false;
  static byte lastObstacleRow = 0;
  static byte obstacleGap = 0;
  static unsigned long frameTimer = 0;

  if (showScore) {
    gameIsActive = false;
    char buf[17];
    snprintf(buf, sizeof(buf), "Coins:%u       ", coins);
    lcdWriteBuffered(0, 0, buf);
    snprintf(buf, sizeof(buf), "Best:%u        ", highScores[0]);
    lcdWriteBuffered(0, 1, buf);

    if (buttonAction == 1 || buttonAction == 2) {
      // increment gamesPlayed and save
      gamesPlayed[0]++;
      saveGamesPlayedToEEPROM(0);
      showScore = false;
      showTitle = true;
      lcdClearBuffered();
    }
    return;
  }

  if (showTitle) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink;
      lastTitleBlink = millis();
      lcdClearBuffered();
      if (titleBlink) {
        lcdWriteBuffered(0, 0, "Runner - Click");
        lcdWriteBuffered(0, 1, "Hold=Back       ");
      } else {
        lcdWriteBuffered(0, 0, "                ");
        lcdWriteBuffered(0, 1, "                ");
      }
    }
    if (buttonAction == 1) {
      initRunnerGraphics();
      heroPos = 1;
      coins = 0;
      newTerrainType = 0;
      newTerrainDuration = 1;
      gameIsActive = true;
      showTitle = false;
      gameStartTime = millis();
      lastObstacleRow = 0;
      obstacleGap = 0;
      lcdClearBuffered();
    }
    if (buttonAction == 2) {
      showTitle = true;
      gameState = STATE_IDLE;
      lcdClearBuffered();
    }
    return;
  }

  // Game frame
  if (!frameReady()) return;

  // generate terrain occasionally
  if (--newTerrainDuration == 0) {
    if (newTerrainType == 0) {
      if (obstacleGap > 0) {
        obstacleGap--;
        newTerrainType = (random(2) == 0) ? 3 : 4; // coin
        newTerrainDuration = 2;
      } else {
        int r = random(10);
        if (r < 6) {
          newTerrainType = (random(2) == 0) ? 3 : 4; // coin
          newTerrainDuration = 2;
        } else {
          if (lastObstacleRow == 1) { newTerrainType = 2; lastObstacleRow = 2; }
          else { newTerrainType = 1; lastObstacleRow = 1; }
          newTerrainDuration = 2;
          obstacleGap = 3;
        }
      }
    } else {
      newTerrainType = 0;
      newTerrainDuration = 3 + random(3);
    }
  }

  advanceTerrain(terrainLower, (newTerrainType == 1) ? 5 : (newTerrainType == 3 ? 4 : ' '));
  advanceTerrain(terrainUpper, (newTerrainType == 2) ? 5 : (newTerrainType == 4 ? 4 : ' '));

  // input
  if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause) {
    if (heroPos <= 2) heroPos = 3;
  }

  // collision detection at player column (HERO_HORIZONTAL_POSITION)
  char upperSave = terrainUpper[HERO_HORIZONTAL_POSITION];
  char lowerSave = terrainLower[HERO_HORIZONTAL_POSITION];
  char upper = ' ', lower = ' ';
  bool collide = false;
  bool gotCoin = false;

  // hero vertical mapping
  if (heroPos <= 2) { upper = ' '; lower = heroPos; }
  else if (heroPos >= 5 && heroPos <= 8) { upper = 3; lower = ' '; }
  else if (heroPos >= 11) { upper = (heroPos == 11) ? 1 : 2; lower = ' '; }
  else { upper = ' '; lower = 3; }

  if (upperSave == 4 && upper != ' ') { gotCoin = true; upperSave = ' '; }
  if (lowerSave == 4 && lower != ' ') { gotCoin = true; lowerSave = ' '; }
  if (gotCoin) coins++;

  if (upper != ' ') {
    collide = (upperSave == 5 || upperSave == 6 || upperSave == 7);
    terrainUpper[HERO_HORIZONTAL_POSITION] = upper;
  }
  if (lower != ' ') {
    collide |= (lowerSave == 5 || lowerSave == 6 || lowerSave == 7);
    terrainLower[HERO_HORIZONTAL_POSITION] = lower;
  }

  // draw terrain (optimized diff)
  // top row
  for (int i = 0; i < TERRAIN_WIDTH; ++i) {
    char ch = terrainUpper[i];
    char curr = lcdBuf[0][i];
    char outCh = (ch == ' ' ? ' ' : (ch == 4 ? '*' : (ch == 5 ? '#' : (ch == 6 ? '>' : (ch == 7 ? '<' : '?')))));
    if (curr != outCh) {
      lcdWriteBufferedChar(i, 0, outCh);
    }
  }
  // bottom row
  for (int i = 0; i < TERRAIN_WIDTH; ++i) {
    char ch = terrainLower[i];
    char curr = lcdBuf[1][i];
    char outCh = (ch == ' ' ? ' ' : (ch == 4 ? '*' : (ch == 5 ? '#' : (ch == 6 ? '>' : (ch == 7 ? '<' : '?')))));
    if (curr != outCh) {
      lcdWriteBufferedChar(i, 1, outCh);
    }
  }
  // HUD
  char hud[17];
  snprintf(hud, sizeof(hud), "Coins:%u   Hi:%u", coins, highScores[0]);
  lcdWriteBuffered(0, 0, hud);

  // restore terrain
  terrainUpper[HERO_HORIZONTAL_POSITION] = upperSave;
  terrainLower[HERO_HORIZONTAL_POSITION] = lowerSave;

  if (collide) {
    // game over
    gameIsActive = false;
    // update stats
    // save highscore
    if (coins > highScores[0]) {
      highScores[0] = coins;
      saveHighScoreToEEPROM(0);
      // show new record
      lcdClearBuffered();
      lcdWriteBuffered(0, 0, "Game Over!");
      lcdWriteBuffered(0, 1, "NEW HIGH SCORE!");
      // small blocking to show message (short)
      unsigned long t0 = millis();
      while (millis() - t0 < 1200) { updateButton(); /* keep responsive*/ }
    } else {
      lcdClearBuffered();
      lcdWriteBuffered(0, 0, "Game Over!");
      lcdWriteBuffered(0, 1, "Press to cont.");
      unsigned long t0 = millis();
      while (millis() - t0 < 700) { updateButton(); }
    }
    // stats
    gamesPlayed[0]++;
    saveGamesPlayedToEEPROM(0);
    totalPlayTime[0] += (millis() - gameStartTime) / 1000;
    saveTotalPlayTimeToEEPROM(0);
    showTitle = true;
    // show score page
    lcdClearBuffered();
    char s1[17];
    snprintf(s1, sizeof(s1), "Coins:%u   Best:%u", coins, highScores[0]);
    lcdWriteBuffered(0, 0, s1);
    showTitle = true;
    return;
  }

  // hero animation
  if (heroPos == 2 || heroPos == 10) heroPos = 1;
  else if ((heroPos >= 5 && heroPos <= 7) && (terrainLower[HERO_HORIZONTAL_POSITION] == 5 || terrainLower[HERO_HORIZONTAL_POSITION] == 6 || terrainLower[HERO_HORIZONTAL_POSITION] == 7)) heroPos = 11;
  else if (heroPos >= 11 && terrainLower[HERO_HORIZONTAL_POSITION] == ' ') heroPos = 7;
  else if (heroPos == 12) heroPos = 11;
  else ++heroPos;
}

// ---------- SNAKE (full-featured, 1-button controls) ----------
static byte snakeX[32], snakeY[32];
static byte snakeLen = 0;
static byte foodX = 0, foodY = 0;
static byte snakeDir = 0; // 0 = up, 1 = down

void initSnakeGraphics() {
  // starting snake
  snakeLen = 3;
  for (byte i = 0; i < snakeLen; ++i) { snakeX[i] = 5 - i; snakeY[i] = 0; }
  // spawn food
  bool valid = false;
  do {
    valid = true;
    foodX = random(16);
    foodY = random(2);
    for (byte i = 0; i < snakeLen; ++i) if (snakeX[i] == foodX && snakeY[i] == foodY) valid = false;
  } while (!valid);
}

void snakeGame() {
  static bool showTitle = true;
  static unsigned long lastMove = 0;
  static unsigned long lastTitleBlink = 0;
  static bool titleBlink = false;
  static unsigned int score = 0;
  static bool showScore = false;

  if (showScore) {
    gameIsActive = false;
    char buf[17];
    snprintf(buf, sizeof(buf), "Score:%u   High:%u", score, highScores[1]);
    lcdWriteBuffered(0, 0, buf);
    lcdWriteBuffered(0, 1, "Press to cont.");
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[1]++;
      saveGamesPlayedToEEPROM(1);
      showScore = false;
      showTitle = true;
      lcdClearBuffered();
    }
    return;
  }

  if (showTitle) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink;
      lastTitleBlink = millis();
      lcdClearBuffered();
      if (titleBlink) {
        lcdWriteBuffered(0, 0, "Snake - Click");
        lcdWriteBuffered(0, 1, "Hold=Back      ");
      }
    }
    if (buttonAction == 1) {
      initSnakeGraphics();
      score = 0;
      snakeDir = 0;
      showTitle = false;
      gameIsActive = true;
      gameStartTime = millis();
      lcdClearBuffered();
    }
    if (buttonAction == 2) {
      gameState = STATE_IDLE;
      lcdClearBuffered();
    }
    return;
  }

  // Flip direction on click
  if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause) {
    snakeDir = (snakeDir == 0) ? 1 : 0;
  }

  // speed by difficulty
  int baseSpeed = (difficultyPerGame[1] == 0) ? 300 : (difficultyPerGame[1] == 1 ? 220 : 160);
  if (millis() - lastMove < (unsigned long)(baseSpeed - score * 3)) return;
  lastMove = millis();

  // move head
  byte newX = snakeX[0] + 1;
  byte newY = snakeY[0];
  if (snakeDir == 0 && newY > 0) newY--;
  else if (snakeDir == 1 && newY < 1) newY++;
  if (newX > 15) newX = 0;

  // collision with self
  for (byte i = 1; i < snakeLen; ++i) if (snakeX[i] == newX && snakeY[i] == newY) {
    // game over
    totalPlayTime[1] += (millis() - gameStartTime) / 1000;
    // score is length-3
    uint16_t s = snakeLen - 3;
    if (s > highScores[1]) { highScores[1] = s; saveHighScoreToEEPROM(1); }
    gamesPlayed[1]++;
    saveGamesPlayedToEEPROM(1);
    saveTotalPlayTimeToEEPROM(1);
    showScore = true;
    return;
  }

  for (byte i = snakeLen; i > 0; --i) { snakeX[i] = snakeX[i - 1]; snakeY[i] = snakeY[i - 1]; }
  snakeX[0] = newX; snakeY[0] = newY;

  if (newX == foodX && newY == foodY) {
    snakeLen++;
    score++;
    // spawn new food
    bool valid;
    do {
      valid = true;
      foodX = random(16);
      foodY = random(2);
      for (byte i = 0; i < snakeLen; ++i) if (snakeX[i] == foodX && snakeY[i] == foodY) valid = false;
    } while (!valid);
  }

  // draw - clear last frame and draw snake
  lcdClearBuffered();
  for (byte i = 0; i < snakeLen; ++i) {
    lcdWriteBufferedChar(snakeX[i], snakeY[i], (i == 0) ? 'O' : 'o');
  }
  lcdWriteBufferedChar(foodX, foodY, '*');
  char hud[17]; snprintf(hud, sizeof(hud), "Len:%u  Hi:%u    ", score, highScores[1]);
  lcdWriteBuffered(0, 0, hud);
}

// ---------- REACTION ----------
void reactionGame() {
  static byte phase = 0; // 0=title,1=waiting random,2=now,3=show
  static unsigned long waitStart = 0;
  static unsigned long targetTime = 0;
  static unsigned int bestTime = 99999;
  static unsigned long gameStart = 0;

  if (phase == 0) {
    gameIsActive = false;
    lcdClearBuffered();
    lcdWriteBuffered(0, 0, "Reaction");
    lcdWriteBuffered(0, 1, "Click=start");
    if (buttonAction == 1) {
      phase = 1;
      waitStart = millis();
      targetTime = random(1000, 3000);
      lcdClearBuffered();
      lcdWriteBuffered(0, 0, "Wait...");
      gameIsActive = true;
      gameStart = millis();
    } else if (buttonAction == 2) {
      gameState = STATE_IDLE;
      lcdClearBuffered();
    }
    return;
  }

  if (phase == 1) {
    updateButton();
    if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause) {
      // too early
      lcdClearBuffered();
      lcdWriteBuffered(0, 0, "Too Early!");
      // count playtime
      totalPlayTime[2] += (millis() - gameStart) / 1000;
      saveTotalPlayTimeToEEPROM(2);
      phase = 0;
      gameIsActive = false;
      unsigned long t0 = millis();
      while (millis() - t0 < 800) { updateButton(); } // short pause
      lcdClearBuffered();
      return;
    }
    if (millis() - waitStart >= targetTime) {
      phase = 2;
      lcdClearBuffered();
      lcdWriteBuffered(0, 0, "NOW!");
      waitStart = millis();
    }
    return;
  }

  if (phase == 2) {
    if (digitalRead(PIN_BUTTON) == LOW) {
      unsigned int reaction = millis() - waitStart;
      char buf[17];
      snprintf(buf, sizeof(buf), "Time:%u ms    ", reaction);
      lcdClearBuffered();
      lcdWriteBuffered(0, 0, buf);
      if (reaction < bestTime) bestTime = reaction;
      if (highScores[2] == 0 || reaction < highScores[2]) {
        highScores[2] = reaction;
        saveHighScoreToEEPROM(2);
        lcdWriteBuffered(0, 1, "NEW RECORD!");
      }
      totalPlayTime[2] += (millis() - gameStart) / 1000;
      saveTotalPlayTimeToEEPROM(2);
      gamesPlayed[2]++;
      saveGamesPlayedToEEPROM(2);
      gameIsActive = false;
      phase = 0;
      unsigned long t0 = millis();
      while (millis() - t0 < 1500) { updateButton(); }
      lcdClearBuffered();
    } else if (millis() - waitStart > 3000) {
      lcdClearBuffered();
      lcdWriteBuffered(0, 0, "Too slow!");
      totalPlayTime[2] += (millis() - gameStart) / 1000;
      saveTotalPlayTimeToEEPROM(2);
      gamesPlayed[2]++;
      saveGamesPlayedToEEPROM(2);
      gameIsActive = false;
      phase = 0;
      unsigned long t0 = millis();
      while (millis() - t0 < 1000) { updateButton(); }
      lcdClearBuffered();
    }
    return;
  }
}

// ---------- PONG (simple 1-button) ----------
void pongGame() {
  static bool showTitle = true;
  static unsigned long lastMove = 0;
  static int ballX = 8, ballY = 0;
  static int ballDX = 1, ballDY = 1;
  static byte paddleY = 0;
  static byte cpuY = 1;
  static int score = 0, cpuScore = 0;
  static bool showScore = false;
  static unsigned long gameStart = 0;

  if (showScore) {
    gameIsActive = false;
    char buf[17];
    if (score > cpuScore) snprintf(buf, sizeof(buf), "YOU WIN %d-%d     ", score, cpuScore);
    else snprintf(buf, sizeof(buf), "YOU LOSE %d-%d    ", score, cpuScore);
    lcdWriteBuffered(0, 0, buf);
    char hi[17]; snprintf(hi, sizeof(hi), "Best:%u P:%u", highScores[3], gamesPlayed[3]);
    lcdWriteBuffered(0, 1, hi);
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[3]++;
      saveGamesPlayedToEEPROM(3);
      showScore = false;
      showTitle = true;
      score = cpuScore = 0;
      lcdClearBuffered();
    }
    return;
  }

  if (showTitle) {
    gameIsActive = false;
    static bool blink = false;
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 500) { blink = !blink; lastBlink = millis(); }
    if (blink) { lcdWriteBuffered(0, 0, "PONG - Click"); lcdWriteBuffered(0, 1, "Hold=Back"); }
    else { lcdWriteBuffered(0, 0, "              "); lcdWriteBuffered(0, 1, "           "); }
    if (buttonAction == 1) {
      ballX = 8; ballY = random(2);
      ballDX = (random(2) == 0) ? -1 : 1;
      ballDY = (random(2) == 0) ? -1 : 1;
      paddleY = 0;
      cpuY = 1;
      score = cpuScore = 0;
      showTitle = false;
      gameIsActive = true;
      gameStart = millis();
      lcdClearBuffered();
    }
    if (buttonAction == 2) {
      showTitle = true;
      gameState = STATE_IDLE;
      lcdClearBuffered();
    }
    return;
  }

  if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause) {
    paddleY = (paddleY == 0) ? 1 : 0; // toggle
    // small debounce
    unsigned long t0 = millis();
    while (millis() - t0 < 120) updateButton();
  }

  if (millis() - lastMove < 180) return;
  lastMove = millis();

  if (ballX > 8 && ballDX > 0 && random(10) < 7) cpuY = ballY;
  ballX += ballDX;
  ballY += ballDY;
  if (ballY >= 1) { ballY = 1; ballDY = -1; }
  if (ballY <= 0) { ballY = 0; ballDY = 1; }

  // collision with player paddle
  if (ballX == 1 && ballY == paddleY && ballDX < 0) {
    ballDX = 1;
    ballDY = (random(2) == 0) ? -1 : 1;
  }
  // collision with cpu paddle
  if (ballX == 14 && ballY == cpuY && ballDX > 0) {
    ballDX = -1;
    ballDY = (random(2) == 0) ? -1 : 1;
  }

  if (ballX <= 0 && ballDX < 0) {
    cpuScore++;
    if (cpuScore >= 5) {
      totalPlayTime[3] += (millis() - gameStart) / 1000;
      if (score > highScores[3]) { highScores[3] = score; saveHighScoreToEEPROM(3); }
      saveTotalPlayTimeToEEPROM(3);
      gamesPlayed[3]++;
      saveGamesPlayedToEEPROM(3);
      lcdClearBuffered();
      showScore = true;
      return;
    }
    ballX = 8; ballY = random(2);
    ballDX = 1; ballDY = (random(2) == 0) ? -1 : 1;
  }

  if (ballX >= 15 && ballDX > 0) {
    score++;
    if (score >= 5) {
      totalPlayTime[3] += (millis() - gameStart) / 1000;
      if (score > highScores[3]) { highScores[3] = score; saveHighScoreToEEPROM(3); }
      saveTotalPlayTimeToEEPROM(3);
      gamesPlayed[3]++;
      saveGamesPlayedToEEPROM(3);
      lcdClearBuffered();
      showScore = true;
      return;
    }
    ballX = 8; ballY = random(2);
    ballDX = -1; ballDY = (random(2) == 0) ? -1 : 1;
  }

  // draw
  lcdClearBuffered();
  // paddles
  lcdWriteBufferedChar(0, paddleY, '|');
  lcdWriteBufferedChar(15, cpuY, '|');
  // ball
  if (ballX >= 0 && ballX < 16 && ballY >= 0 && ballY < 2)
    lcdWriteBufferedChar(ballX, ballY, 'o');
  char scorebuf[17]; snprintf(scorebuf, sizeof(scorebuf), "%d - %d      Hi:%u", score, cpuScore, highScores[3]);
  lcdWriteBuffered(0, 0, scorebuf);
}

// ---------- MEMORY (Simon-like top/bottom) ----------
void memoryGame() {
  static byte sequence[48];
  static byte seqLen = 0;
  static byte playerPos = 0;
  static byte phase = 0; // 0=title,1=show,2=input,3=score
  static unsigned long lastAction = 0;
  static unsigned long showStart = 0;
  static bool titleBlink = false;
  static unsigned long lastTitleBlink = 0;
  static unsigned int level = 0;

  if (phase == 3) {
    gameIsActive = false;
    char buf[17]; snprintf(buf, sizeof(buf), "Level:%u Best:%u", level, highScores[4]);
    lcdWriteBuffered(0, 0, buf);
    lcdWriteBuffered(0, 1, "Press to cont.");
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[4]++; saveGamesPlayedToEEPROM(4);
      phase = 0; seqLen = 0; lcdClearBuffered();
    }
    return;
  }

  if (phase == 0) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink; lastTitleBlink = millis();
      if (titleBlink) { lcdWriteBuffered(0, 0, "Memory - Click"); lcdWriteBuffered(0, 1, "Hold=Back"); }
      else lcdClearBuffered();
    }
    if (buttonAction == 1) {
      seqLen = 0; playerPos = 0; level = 0;
      phase = 1; showStart = millis();
      gameIsActive = true; gameStartTime = millis();
      lcdClearBuffered();
    }
    if (buttonAction == 2) { gameState = STATE_IDLE; lcdClearBuffered(); }
    return;
  }

  if (phase == 1) {
    // add a new random top/bottom (0 or 1)
    if (seqLen < 48) {
      sequence[seqLen++] = random(2);
    }
    // show sequence
    if (millis() - showStart > 500) {
      // display items sequentially
      static byte showIndex = 0;
      if (showIndex < seqLen) {
        lcdClearBuffered();
        if (sequence[showIndex] == 0) lcdWriteBuffered(6, 0, "**");
        else lcdWriteBuffered(6, 1, "**");
        showIndex++;
        showStart = millis();
      } else {
        // done showing
        showIndex = 0;
        phase = 2;
        playerPos = 0;
        lcdClearBuffered();
        lcdWriteBuffered(0, 0, "Your turn!");
      }
    }
    return;
  }

  if (phase == 2) {
    // prompt shows progress
    char buf[17]; snprintf(buf, sizeof(buf), "Done:%u/%u      ", playerPos, seqLen);
    lcdWriteBuffered(0, 1, buf);
    if (buttonAction == 1) {
      // top (0)
      if (0 == sequence[playerPos]) {
        playerPos++;
        lcdClearBuffered();
        lcdWriteBuffered(0, 0, "Correct!");
        delay(200);
        if (playerPos >= seqLen) { level++; phase = 1; lcdClearBuffered(); }
      } else {
        // wrong
        totalPlayTime[4] += (millis() - gameStartTime) / 1000; saveTotalPlayTimeToEEPROM(4);
        if (level > highScores[4]) { highScores[4] = level; saveHighScoreToEEPROM(4); }
        phase = 3; lcdClearBuffered(); lcdWriteBuffered(0, 0, "Wrong!"); delay(400);
      }
    } else if (buttonAction == 2) {
      // bottom (1)
      if (1 == sequence[playerPos]) {
        playerPos++;
        lcdClearBuffered();
        lcdWriteBuffered(0, 0, "Correct!"); delay(200);
        if (playerPos >= seqLen) { level++; phase = 1; lcdClearBuffered(); }
      } else {
        totalPlayTime[4] += (millis() - gameStartTime) / 1000; saveTotalPlayTimeToEEPROM(4);
        if (level > highScores[4]) { highScores[4] = level; saveHighScoreToEEPROM(4); }
        phase = 3; lcdClearBuffered(); lcdWriteBuffered(0, 0, "Wrong!"); delay(400);
      }
    }
    return;
  }
}

// ---------- FLAPPY (fuller than earlier) ----------
void flappyGame() {
  static bool showTitle = true;
  static unsigned long lastMove = 0;
  static byte birdY = 0;
  static byte pipeX = 15;
  static byte pipeGap = 0;
  static unsigned int score = 0;
  static byte lives = 3;
  static bool showScore = false;
  static unsigned long gameStart = 0;
  static bool btnPressed = false;
  static unsigned long lastFlap = 0;
  static bool hitPipe = false;
  static unsigned long lastAnim = 0;
  static byte birdFrame = 0;

  if (showScore) {
    gameIsActive = false;
    char buf[17]; snprintf(buf, sizeof(buf), "Score:%u Best:%u", score, highScores[5]);
    lcdWriteBuffered(0, 0, buf);
    lcdWriteBuffered(0, 1, "Press to cont.");
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[5]++; saveGamesPlayedToEEPROM(5);
      showScore = false; showTitle = true;
      lives = 3; score = 0; lcdClearBuffered();
    }
    return;
  }

  if (showTitle) {
    gameIsActive = false;
    static bool blink = false;
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 500) { blink = !blink; lastBlink = millis(); }
    if (blink) { lcdWriteBuffered(0, 0, "Flappy - Click"); lcdWriteBuffered(0, 1, "Hold=Back     "); }
    else lcdClearBuffered();
    if (buttonAction == 1) {
      // start
      birdY = 0; pipeX = 15; pipeGap = random(2); score = 0; lives = 3;
      showTitle = false; gameIsActive = true; gameStart = millis(); hitPipe = false;
      lastFlap = millis(); lastAnim = millis(); lcdClearBuffered();
    }
    if (buttonAction == 2) { gameState = STATE_IDLE; lcdClearBuffered(); }
    return;
  }

  // input: flap on click
  bool currentBtn = (digitalRead(PIN_BUTTON) == LOW);
  if (currentBtn && !btnPressed && !holdingForPause) {
    birdY = 0;
    btnPressed = true;
    lastFlap = millis();
    birdFrame = 0;
  }
  if (!currentBtn) btnPressed = false;

  // gravity & movement every frame period
  if (!frameReady()) return;

  // gravity: after some time, bird falls
  if (millis() - lastFlap > 400) birdY = 1;

  // move pipe
  if (pipeX > 0) pipeX--;
  else { pipeX = 15; pipeGap = random(2); score++; }

  // collision check at x==2
  if (pipeX == 2 && !hitPipe) {
    bool hit = false;
    if (pipeGap == 0 && birdY == 0) hit = true;
    if (pipeGap == 1 && birdY == 1) hit = true;
    if (hit) {
      lives--; hitPipe = true;
      lcdClearBuffered();
      char crash[17]; snprintf(crash, sizeof(crash), "CRASH! Lives:%u", lives);
      lcdWriteBuffered(0, 0, crash);
      delay(800);
      if (lives == 0) {
        totalPlayTime[5] += (millis() - gameStart) / 1000; saveTotalPlayTimeToEEPROM(5);
        if (score > highScores[5]) { highScores[5] = score; saveHighScoreToEEPROM(5); }
        gamesPlayed[5]++; saveGamesPlayedToEEPROM(5);
        showScore = true; return;
      }
      // reset pipe and bird
      pipeX = 15; birdY = 0; hitPipe = false; lcdClearBuffered();
    }
  }

  // draw
  lcdClearBuffered();
  // bird as custom char? fallback to '>'
  if (birdY < 2) lcdWriteBufferedChar(2, birdY, '>');
  if (pipeGap == 0) lcdWriteBufferedChar(pipeX, 0, '#'); else lcdWriteBufferedChar(pipeX, 1, '#');
  char hud[17]; snprintf(hud, sizeof(hud), "Sc:%u Lives:%u Hi:%u", score, lives, highScores[5]);
  lcdWriteBuffered(0, 0, hud);
}

// ---------- STOPWATCH (simple) ----------
void stopwatchApp() {
  static bool running = false;
  static unsigned long startTime = 0;
  static unsigned long elapsed = 0;
  static bool initialized = false;

  if (!initialized) { lcdClearBuffered(); initialized = true; }
  if (buttonAction == 1) {
    if (!running) { running = true; startTime = millis() - elapsed; }
    else { running = false; elapsed = millis() - startTime; }
  }
  if (buttonAction == 2) {
    if (elapsed == 0 && !running) { initialized = false; gameState = STATE_MAIN_MENU; drawMainMenu(); return; }
    else { elapsed = 0; running = false; lcdClearBuffered(); }
  }
  if (running) elapsed = millis() - startTime;
  unsigned long secs = elapsed / 1000; unsigned long mins = secs / 60; secs %= 60; unsigned long ms = (elapsed % 1000) / 10;
  char buf[17]; snprintf(buf, sizeof(buf), "Stopwatch %02lu:%02lu:%02lu", mins, secs, ms);
  lcdWriteBuffered(0, 0, "Stopwatch");
  lcdWriteBuffered(2, 1, buf + 10); // prints time part
}

// ---------- CALCULATOR (simple) ----------
void calculatorApp() {
  static long num1 = 0, num2 = 0; static byte operation = 0; static bool enteringNum2 = false; static bool init = false;
  if (!init) { num1 = 0; num2 = 0; operation = 0; enteringNum2 = false; init = true; lcdClearBuffered(); }
  if (buttonAction == 1) {
    if (!enteringNum2) { num1++; if (num1 > 9999) num1 = -9999; } else { num2++; if (num2 > 9999) num2 = -9999; }
  }
  if (buttonAction == 2 && operation == 0 && !enteringNum2) { init = false; gameState = STATE_MAIN_MENU; drawMainMenu(); return; }
  if (buttonAction == 2 && operation > 0) {
    if (!enteringNum2) { enteringNum2 = true; num2 = 0; }
    else {
      long result = 0;
      switch (operation) { case 1: result = num1 + num2; break; case 2: result = num1 - num2; break; case 3: result = num1 * num2; break; case 4: result = (num2 != 0) ? num1 / num2 : 0; break; }
      lcdClearBuffered();
      char buf[17]; snprintf(buf, sizeof(buf), "Result:%ld", result);
      lcdWriteBuffered(0, 0, buf);
      num1 = result; num2 = 0; operation = 0; enteringNum2 = false;
      unsigned long t0 = millis(); while (millis() - t0 < 800) updateButton();
      lcdClearBuffered();
    }
  }
  // cycle ops on long click pattern
  if (buttonAction == 1 && !enteringNum2) {
    // toggle operation quickly if button is kept pressed
    operation++; if (operation > 4) operation = 1;
    char ops[] = " +-*/";
    char buf[17]; snprintf(buf, sizeof(buf), "Op: %c", ops[operation]);
    lcdWriteBuffered(0, 0, buf);
    delay(250);
    lcdClearBuffered();
  }
  // display
  char line2[17];
  if (!enteringNum2) snprintf(line2, sizeof(line2), "%ld        ", num1);
  else {
    char ops[] = " +-*/";
    snprintf(line2, sizeof(line2), "%ld %c %ld", num1, ops[operation], num2);
  }
  lcdWriteBuffered(0, 0, "Calc [Hold=Exit]");
  lcdWriteBuffered(0, 1, line2);
}

// ---------- DICE ----------
void diceApp() {
  static byte sides = 6; static int res = 0; static bool init = false;
  if (!init) { sides = 6; res = 0; init = true; lcdClearBuffered(); }
  char line1[17]; snprintf(line1, sizeof(line1), "Dice: D%u [H=Exit]", sides); lcdWriteBuffered(0, 0, line1);
  if (buttonAction == 1) { sides += 2; if (sides > 20) sides = 4; res = 0; }
  if (buttonAction == 2) {
    if (res == 0) { init = false; gameState = STATE_MAIN_MENU; drawMainMenu(); return; }
    else {
      for (int i = 0; i < 8; ++i) {
        res = random(1, sides + 1);
        char buf[17]; snprintf(buf, sizeof(buf), "Rolling: %d    ", res);
        lcdWriteBuffered(0, 1, buf);
        unsigned long t0 = millis(); while (millis() - t0 < 80) updateButton();
      }
    }
  }
  if (res > 0) { char buf[17]; snprintf(buf, sizeof(buf), "Result: %d    ", res); lcdWriteBuffered(0, 1, buf); }
  else lcdWriteBuffered(0, 1, "Hold to roll    ");
}

// ---------- SCREENSAVER ----------
void screensaver() {
  static byte x = 0, y = 0; static int dx = 1, dy = 1; static unsigned long lastMove = 0;
  if (lcdBuf[0][0] == '\xff') lcdInitBuffer(); // init detection
  if (millis() - lastMove > 150) {
    lcdClearBuffered();
    lcdWriteBufferedChar(x, y, ' ');
    x += dx; y += dy;
    if (x >= 15) { x = 15; dx = -1; }
    if (x == 0) dx = 1;
    if (y >= 1) { y = 1; dy = -1; }
    if (y == 0) dy = 1;
    lcdWriteBufferedChar(x, y, '*');
    lastMove = millis();
  }
  if (buttonAction == 1 || buttonAction == 2) {
    gameState = STATE_IDLE; lcdClearBuffered();
  }
}

// ---------- STATS APP ----------
void statsApp() {
  static byte page = 0;
  if (buttonAction == 1) { page = (page + 1) % 6; lcdClearBuffered(); }
  if (buttonAction == 2) { gameState = STATE_MAIN_MENU; drawMainMenu(); return; }
  char buf[17];
  snprintf(buf, sizeof(buf), "%s [H=Exit]", gameNames[page]);
  lcdWriteBuffered(0, 0, buf);
  char line2[17];
  snprintf(line2, sizeof(line2), "Hi:%u P:%u T:%us", highScores[page], gamesPlayed[page], totalPlayTime[page]);
  lcdWriteBuffered(0, 1, line2);
}

// ---------- SETTINGS APP ----------
void settingsApp() {
  static byte option = 0; // 0: difficulty select per current game, 1: Reset Stats, 2: Back
  static bool initialized = false;
  if (!initialized) { option = 0; initialized = true; lcdClearBuffered(); }

  if (buttonAction == 1) {
    if (option == 0) {
      // cycle difficulty for currently selected gamesMenuSelection (or mainMenuSelection - choose gamesMenuSelection)
      // We'll use gamesMenuSelection as last selected in Games menu; default to 0
      byte g = gamesMenuSelection;
      difficultyPerGame[g] = (difficultyPerGame[g] + 1) % 3;
      saveDifficultyToEEPROM(g);
    } else if (option == 1) {
      // Reset stats
      for (byte i = 0; i < 6; ++i) {
        highScores[i] = 0; gamesPlayed[i] = 0; totalPlayTime[i] = 0; difficultyPerGame[i] = 1;
        saveHighScoreToEEPROM(i); saveGamesPlayedToEEPROM(i); saveTotalPlayTimeToEEPROM(i); saveDifficultyToEEPROM(i);
      }
      lcdClearBuffered(); lcdWriteBuffered(0, 0, "Scores reset! "); unsigned long t0 = millis(); while (millis()-t0<800) updateButton(); lcdClearBuffered();
    }
  }

  if (buttonAction == 2) {
    option = (option + 1) % 3;
    if (option == 2) { initialized = false; gameState = STATE_MAIN_MENU; drawMainMenu(); return; }
    lcdClearBuffered();
  }

  if (option == 0) {
    char buf[17]; snprintf(buf, sizeof(buf), ">Difficulty g:%u", gamesMenuSelection);
    lcdWriteBuffered(0, 0, buf);
    const char* names[] = {"Easy", "Normal", "Hard"};
    char line2[17]; snprintf(line2, sizeof(line2), "%s             ", names[difficultyPerGame[gamesMenuSelection]]);
    lcdWriteBuffered(0, 1, line2);
  } else if (option == 1) {
    lcdWriteBuffered(0, 0, ">Reset Stats    ");
    lcdWriteBuffered(0, 1, "Click to reset  ");
  }
}

// ---------- UTIL: helper to set boot defaults ----------
void initDefaultsIfFirstRun() {
  // We'll detect if EEPROM has some valid marker; simple approach: if all zeros, write defaults
  bool allZero = true;
  for (int i = 0; i < 6; ++i) if (EEPROM.read(EEPROM_ADDR_GAMESPLAYED + i) != 0) allZero = false;
  if (allZero) {
    for (int i = 0; i < 6; ++i) {
      highScores[i] = 0; gamesPlayed[i] = 0; totalPlayTime[i] = 0; difficultyPerGame[i] = 1;
    }
    saveAllStats();
  }
}

// ---------- setup() and loop() ----------
void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  if (BUZZER_PIN != 255) pinMode(BUZZER_PIN, OUTPUT);
  lcd.init();
  lcd.backlight();
  lcdInitBuffer();
  randomSeed(analogRead(A3));
  loadStatsFromEEPROM();
  initDefaultsIfFirstRun();
  lcdClearBuffered();
  slotMachineBoot();
  gameState = STATE_IDLE;
}

void loop() {
  // update button first
  updateButton();

  // handle states
  switch (gameState) {
    case STATE_BOOT: slotMachineBoot(); break;
    case STATE_IDLE: drawIdle(); break;
    case STATE_MAIN_MENU: updateMainMenu(); break;
    case STATE_RUNNER:
      gameIsActive = true;
      runnerGame();
      // after returning (game over), go to games menu
      gameState = STATE_GAMES_MENU;
      break;
    case STATE_SNAKE:
      gameIsActive = true;
      snakeGame();
      gameState = STATE_GAMES_MENU;
      break;
    case STATE_REACTION:
      gameIsActive = true;
      reactionGame();
      gameState = STATE_GAMES_MENU;
      break;
    case STATE_PONG:
      gameIsActive = true;
      pongGame();
      gameState = STATE_GAMES_MENU;
      break;
    case STATE_MEMORY:
      gameIsActive = true;
      memoryGame();
      gameState = STATE_GAMES_MENU;
      break;
    case STATE_FLAPPY:
      gameIsActive = true;
      flappyGame();
      gameState = STATE_GAMES_MENU;
      break;
    case STATE_SETTINGS: settingsApp(); break;
    case STATE_STOPWATCH: stopwatchApp(); break;
    case STATE_CALCULATOR: calculatorApp(); break;
    case STATE_DICE: diceApp(); break;
    case STATE_SCREENSAVER: screensaver(); break;
    case STATE_STATS: statsApp(); break;
    case STATE_PAUSED: updatePause(); break;
    case STATE_GAMES_MENU: updateGamesMenu(); break;
    default: gameState = STATE_IDLE; break;
  }

  // small idle CPU friendly sleep until next frame
  // but we must remain responsive to the button - loop cycles fast enough
  // rely on frame limiter in games where needed
}
