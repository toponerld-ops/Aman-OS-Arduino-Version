#include "LiquidCrystal_I2C.h"
#include <EEPROM.h>
#include <SoftwareSerial.h>

LiquidCrystal_I2C lcd(0x27,16,2);
SoftwareSerial bluetooth(10, 11); // RX, TX for receiving weather data

#define PIN_BUTTON 2
#define HOLD_TIME 1500
#define PAUSE_HOLD_TIME 2000
#define CLICK_COOLDOWN 500

// States
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
#define STATE_WEATHER 17

#define TERRAIN_WIDTH 16
#define HERO_HORIZONTAL_POSITION 1

static byte gameState = STATE_BOOT;
static byte previousGame = STATE_RUNNER;
static byte mainMenuSelection = 0;
static byte gamesMenuSelection = 0;
static byte pauseSelection = 0;
static unsigned int highScores[6] = {0, 0, 0, 0, 0, 0};
static unsigned long totalPlayTime[6] = {0, 0, 0, 0, 0, 0};
static unsigned int gamesPlayed[6] = {0, 0, 0, 0, 0, 0};

static byte buttonAction = 0;
static unsigned long btnDownTime = 0;
static unsigned long lastClickTime = 0;
static bool btnWasPressed = false;
static bool holdingForPause = false;
static bool gameIsActive = false;

static char terrainUpper[TERRAIN_WIDTH + 1];
static char terrainLower[TERRAIN_WIDTH + 1];
static byte snakeX[32], snakeY[32];
static byte snakeLen, foodX, foodY, snakeDir;

// Weather module data
static int weatherSoil = 0;
static float weatherTemp = 0.0;
static float weatherHumidity = 0.0;
static unsigned long lastWeatherUpdate = 0;
static bool weatherConnected = false;

// Settings
static byte difficulty = 1; // 0=Easy, 1=Normal, 2=Hard
static bool soundEnabled = true;
static unsigned long idleTimeout = 60000; // 60 seconds

const char* mainMenuItems[] = {"Games", "Weather", "Stopwatch", "Calculator", "Dice", "Stats", "Settings"};
const char* gameNames[] = {"Runner", "Snake", "Reaction", "Pong", "Memory", "Flappy"};

// Sprites
byte ippo[8] = {B00110,B00110,B00000,B00111,B00110,B00110,B00110,B01001};
byte ippoJab[8] = {B00110,B00110,B00000,B00111,B00111,B00110,B00110,B01001};
byte ippoDuck[8] = {B00000,B00110,B00110,B00111,B00110,B00110,B01001,B00000};
byte miyata[8] = {B01100,B01100,B00000,B11100,B01100,B01100,B01100,B10010};
byte miyataJab[8] = {B01100,B01100,B00000,B11100,B11100,B01100,B01100,B10010};
byte miyataDuck[8] = {B00000,B01100,B01100,B11100,B01100,B01100,B10010,B00000};
byte spark[8] = {B00000,B00100,B01110,B00100,B00000,B00000,B00000,B00000};

byte heroRun1[8] = {B00000,B01110,B01101,B00110,B11110,B01110,B10010,B00000};
byte heroRun2[8] = {B00000,B01110,B01101,B00110,B11110,B01110,B01100,B00000};
byte heroJump[8] = {B00000,B01110,B01101,B11110,B00010,B01110,B00000,B00000};
byte coin[8] = {B00000,B01110,B11011,B11111,B11111,B11011,B01110,B00000};
byte blockFull[8] = {B11111,B11111,B11111,B11111,B11111,B11111,B11111,B11111};
byte blockRight[8] = {B00011,B00011,B00011,B00011,B00011,B00011,B00011,B00011};
byte blockLeft[8] = {B11000,B11000,B11000,B11000,B11000,B11000,B11000,B11000};

void loadHighScores() {
  for (int i = 0; i < 6; i++) {
    highScores[i] = EEPROM.read(i * 2) | (EEPROM.read(i * 2 + 1) << 8);
    if (highScores[i] > 9999) highScores[i] = 0;
  }
  difficulty = EEPROM.read(20);
  if (difficulty > 2) difficulty = 1;
}

void saveHighScore(byte game, unsigned int score) {
  bool isBetter = (game == 2) ? (score < highScores[game] || highScores[game] == 0) : (score > highScores[game]);
  if (isBetter) {
    highScores[game] = score;
    EEPROM.write(game * 2, score & 0xFF);
    EEPROM.write(game * 2 + 1, (score >> 8) & 0xFF);
  }
}

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
        pauseSelection = 0;
        lcd.clear();
        lcd.setCursor(3, 0);
        lcd.print("PAUSED");
        lcd.setCursor(0, 1);
        lcd.print(">Resume   Quit");
      }
      btnWasPressed = false;
    }
    else if (held >= HOLD_TIME && !gameIsActive && gameState != STATE_PAUSED) {
      buttonAction = 2;
      btnWasPressed = false;
    }
    else if ((gameState == STATE_MAIN_MENU || gameState == STATE_GAMES_MENU) && held < HOLD_TIME && held > 200) {
      int progress = (held * 12) / HOLD_TIME;
      lcd.setCursor(0, 1);
      for (int i = 0; i < 12; i++) {
        lcd.print(i < progress ? "\xFF" : " ");
      }
      float remaining = (HOLD_TIME - held) / 1000.0;
      lcd.setCursor(12, 1);
      lcd.print(remaining, 1);
      lcd.print("s");
    }
  }
  else if (!pressed && btnWasPressed) {
    btnWasPressed = false;
    holdingForPause = false;
    if (gameState == STATE_MAIN_MENU) {
      lcd.setCursor(0, 1);
      lcd.print("Press to select ");
    } else if (gameState == STATE_GAMES_MENU) {
      lcd.setCursor(0, 1);
      lcd.print("Hi:");
      lcd.print(highScores[gamesMenuSelection]);
      lcd.print("         ");
    }
    unsigned long held = now - btnDownTime;
    if (held < HOLD_TIME && now - lastClickTime > CLICK_COOLDOWN) {
      buttonAction = 1;
      lastClickTime = now;
    }
  }
}

void slotMachineBoot() {
  const char* target = "AMAN OS";
  char display[8] = "       ";
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("WELCOME!");
  delay(800);
  lcd.clear();
  
  for (int i = 0; i < 7; i++) {
    if (target[i] == ' ') { display[i] = ' '; continue; }
    for (int spin = 0; spin < 15; spin++) {
      display[i] = 'A' + random(26);
      lcd.setCursor(4, 0);
      lcd.print(display);
      delay(50 + spin * 10);
    }
    display[i] = target[i];
    lcd.setCursor(4, 0);
    lcd.print(display);
    delay(200);
  }
  
  for (int i = 0; i < 3; i++) {
    lcd.noBacklight(); delay(100);
    lcd.backlight(); delay(100);
  }
  
  lcd.setCursor(2, 1);
  lcd.print("Loading...");
  delay(800);
  lcd.clear();
  gameState = STATE_IDLE;
}

void playIdleFight(bool corner) {
  static unsigned long lastAnim = 0;
  static byte frame = 0;
  if (millis() - lastAnim < 500) return;
  lastAnim = millis();
  
  lcd.createChar(1, ippo);
  lcd.createChar(2, miyata);
  lcd.createChar(3, spark);
  lcd.createChar(4, ippoJab);
  lcd.createChar(5, miyataJab);
  lcd.createChar(6, ippoDuck);
  lcd.createChar(7, miyataDuck);
  
  if (corner) {
    lcd.setCursor(13, 1);
    lcd.print("   ");
    switch(frame) {
      case 0: case 4:
        lcd.setCursor(13, 1); lcd.write(1);
        lcd.setCursor(15, 1); lcd.write(2);
        break;
      case 1: case 2:
        lcd.setCursor(13, 1); lcd.write(4);
        lcd.setCursor(14, 1); lcd.write(3);
        lcd.setCursor(15, 1); lcd.write(2);
        break;
      case 3:
        lcd.setCursor(13, 1); lcd.write(4);
        lcd.setCursor(15, 1); lcd.write(7);
        break;
      case 5: case 6:
        lcd.setCursor(13, 1); lcd.write(1);
        lcd.setCursor(14, 1); lcd.write(3);
        lcd.setCursor(15, 1); lcd.write(5);
        break;
      case 7:
        lcd.setCursor(13, 1); lcd.write(6);
        lcd.setCursor(15, 1); lcd.write(5);
        break;
    }
  } else {
    lcd.setCursor(4, 1);
    lcd.print("        ");
    switch(frame) {
      case 0: case 4:
        lcd.setCursor(5, 1); lcd.write(1);
        lcd.setCursor(10, 1); lcd.write(2);
        break;
      case 1:
        lcd.setCursor(6, 1); lcd.write(1);
        lcd.setCursor(10, 1); lcd.write(2);
        break;
      case 2:
        lcd.setCursor(6, 1); lcd.write(4);
        lcd.setCursor(7, 1); lcd.write(3);
        lcd.setCursor(10, 1); lcd.write(2);
        break;
      case 3:
        lcd.setCursor(6, 1); lcd.write(4);
        lcd.setCursor(10, 1); lcd.write(7);
        break;
      case 5:
        lcd.setCursor(5, 1); lcd.write(1);
        lcd.setCursor(9, 1); lcd.write(2);
        break;
      case 6:
        lcd.setCursor(5, 1); lcd.write(1);
        lcd.setCursor(8, 1); lcd.write(3);
        lcd.setCursor(9, 1); lcd.write(5);
        break;
      case 7:
        lcd.setCursor(5, 1); lcd.write(6);
        lcd.setCursor(9, 1); lcd.write(5);
        break;
    }
  }
  frame = (frame + 1) % 8;
}

void updateIdle() {
  static bool init = false;
  static unsigned long lastActivity = 0;
  
  if (!init) {
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("AMAN OS");
    init = true;
    lastActivity = millis();
  }
  
  playIdleFight(false);
  
  if (buttonAction == 1 || buttonAction == 2) {
    init = false;
    lastActivity = millis();
    gameState = STATE_MAIN_MENU;
    lcd.clear();
    drawMainMenu();
  }
  
  if (millis() - lastActivity > idleTimeout) {
    gameState = STATE_SCREENSAVER;
    lastActivity = millis();
    lcd.clear();
  }
}

void drawMainMenu() {
  lcd.setCursor(0, 0);
  lcd.print(">");
  lcd.print(mainMenuItems[mainMenuSelection]);
  lcd.print("        ");
  lcd.setCursor(0, 1);
  lcd.print("Press to select ");
}

void updateMainMenu() {
  if (buttonAction == 1) {
    mainMenuSelection = (mainMenuSelection + 1) % 7;
    drawMainMenu();
  }
  else if (buttonAction == 2) {
    lcd.clear();
    switch(mainMenuSelection) {
      case 0: gameState = STATE_GAMES_MENU; drawGamesMenu(); break;
      case 1: gameState = STATE_WEATHER; break;
      case 2: gameState = STATE_STOPWATCH; break;
      case 3: gameState = STATE_CALCULATOR; break;
      case 4: gameState = STATE_DICE; break;
      case 5: gameState = STATE_STATS; break;
      case 6: gameState = STATE_SETTINGS; break;
    }
  }
}

void drawGamesMenu() {
  lcd.setCursor(0, 0);
  lcd.print(">");
  lcd.print(gameNames[gamesMenuSelection]);
  lcd.print("        ");
  lcd.setCursor(0, 1);
  lcd.print("Hi:");
  lcd.print(highScores[gamesMenuSelection]);
  lcd.print("         ");
}

void updateGamesMenu() {
  playIdleFight(true);
  if (buttonAction == 1) {
    gamesMenuSelection = (gamesMenuSelection + 1) % 6;
    drawGamesMenu();
  }
  else if (buttonAction == 2) {
    lcd.clear();
    if (gamesMenuSelection == 0) gameState = STATE_RUNNER;
    else if (gamesMenuSelection == 1) gameState = STATE_SNAKE;
    else if (gamesMenuSelection == 2) gameState = STATE_REACTION;
    else if (gamesMenuSelection == 3) gameState = STATE_PONG;
    else if (gamesMenuSelection == 4) gameState = STATE_MEMORY;
    else if (gamesMenuSelection == 5) gameState = STATE_FLAPPY;
  }
}

static unsigned long pauseHoldStart = 0;
void updatePause() {
  if (buttonAction == 1) {
    pauseSelection = (pauseSelection + 1) % 2;
    pauseHoldStart = millis();
    lcd.setCursor(0, 1);
    if (pauseSelection == 0) lcd.print(">Resume   Quit  ");
    else lcd.print(" Resume  >Quit  ");
  }
  
  if (pauseHoldStart > 0) {
    unsigned long elapsed = millis() - pauseHoldStart;
    int progress = (elapsed * 16) / HOLD_TIME;
    if (progress > 16) progress = 16;
    
    lcd.setCursor(0, 0);
    if (pauseSelection == 0) lcd.print("RESUME?");
    else lcd.print("QUIT?  ");
    for (int i = 0; i < 9; i++) {
      lcd.print(i < (progress * 9 / 16) ? "\xFF" : " ");
    }
    
    if (elapsed >= HOLD_TIME) {
      if (pauseSelection == 0) {
        gameState = previousGame;
        lcd.clear();
      } else {
        gameIsActive = false;
        gameState = STATE_IDLE;
        lcd.clear();
      }
      pauseHoldStart = 0;
      return;
    }
  } else {
    lcd.setCursor(0, 0);
    lcd.print("    PAUSED      ");
  }
  
  if (buttonAction == 2) {
    if (pauseSelection == 0) {
      gameState = previousGame;
      lcd.clear();
    } else {
      gameIsActive = false;
      gameState = STATE_IDLE;
      lcd.clear();
    }
    pauseHoldStart = 0;
  }
}

void receiveWeatherData() {
  if (bluetooth.available()) {
    String data = bluetooth.readStringUntil('\n');
    if (data.startsWith("PLANT,")) {
      int comma1 = data.indexOf(',', 6);
      int comma2 = data.indexOf(',', comma1 + 1);
      int comma3 = data.indexOf(',', comma2 + 1);
      if (comma1 > 0 && comma2 > 0) {
        String soilStr = data.substring(6, comma1);
        soilStr.replace("%", "");
        weatherSoil = soilStr.toInt();
        
        String tempStr = data.substring(comma1 + 1, comma2);
        tempStr.replace("C", "");
        weatherTemp = tempStr.toFloat();
        
        String humStr = data.substring(comma2 + 1);
        humStr.replace("%", "");
        weatherHumidity = humStr.toFloat();
        
        lastWeatherUpdate = millis();
        weatherConnected = true;
      }
    }
  }
  if (millis() - lastWeatherUpdate > 15000 && weatherConnected) {
    weatherConnected = false;
  }
}

void weatherApp() {
  static bool initialized = false;
  if (!initialized) {
    lcd.clear();
    initialized = true;
  }
  
  receiveWeatherData();
  
  if (buttonAction == 2) {
    initialized = false;
    gameState = STATE_MAIN_MENU;
    lcd.clear();
    drawMainMenu();
    return;
  }
  
  if (buttonAction == 1) {
    lcd.clear();
  }
  
  lcd.setCursor(0, 0);
  if (weatherConnected) {
    lcd.print("Soil:");
    lcd.print(weatherSoil);
    lcd.print("% T:");
    lcd.print((int)weatherTemp);
    lcd.print("C");
  } else {
    lcd.print("Waiting for data");
  }
  
  lcd.setCursor(0, 1);
  if (weatherConnected) {
    lcd.print("Hum:");
    lcd.print((int)weatherHumidity);
    lcd.print("%");
    if (weatherSoil < 30) {
      lcd.print(" DRY!");
    } else if (weatherSoil > 70) {
      lcd.print(" WET");
    } else {
      lcd.print(" OK");
    }
  } else {
    lcd.print("[H=Exit]");
  }
  
  delay(100);
}// ==================== RUNNER WITH COINS ====================
void initRunnerGraphics() {
  lcd.createChar(1, heroRun1);
  lcd.createChar(2, heroRun2);
  lcd.createChar(3, heroJump);
  lcd.createChar(4, coin);
  lcd.createChar(5, blockFull);
  lcd.createChar(6, blockRight);
  lcd.createChar(7, blockLeft);
  for (int i = 0; i < TERRAIN_WIDTH; ++i) {
    terrainUpper[i] = ' ';
    terrainLower[i] = ' ';
  }
}

void advanceTerrain(char* terrain, byte newTerrain) {
  for (int i = 0; i < TERRAIN_WIDTH; ++i) {
    char current = terrain[i];
    char next = (i == TERRAIN_WIDTH-1) ? newTerrain : terrain[i+1];
    
    if (current == 4) {
      terrain[i] = (i == TERRAIN_WIDTH-1) ? newTerrain : terrain[i+1];
      continue;
    }
    
    switch (current) {
      case ' ': 
        terrain[i] = (next == 5) ? 6 : (next == 4 ? 4 : ' '); 
        break;
      case 5: 
        terrain[i] = (next == ' ' || next == 4) ? 7 : 5; 
        break;
      case 6: 
        terrain[i] = 5; 
        break;
      case 7: 
        terrain[i] = ' '; 
        break;
    }
  }
}

void runnerGame() {
  static byte heroPos = 1;
  static byte newTerrainType = 0, newTerrainDuration = 1;
  static bool playing = false;
  static bool showTitle = true;
  static bool titleBlink = false;
  static unsigned int coins = 0;
  static bool showScore = false;
  static unsigned long lastTitleBlink = 0;
  static unsigned long gameStartTime = 0;
  static byte lastObstacleRow = 0;
  static byte obstacleGap = 0;
  
  if (showScore) {
    gameIsActive = false;
    lcd.setCursor(0, 0);
    lcd.print("Coins:");
    lcd.print(coins);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print("Best:");
    lcd.print(highScores[0]);
    lcd.print("         ");
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[0]++;
      showScore = false;
      showTitle = true;
      lcd.clear();
    }
    return;
  }
  
  if (showTitle) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink;
      lastTitleBlink = millis();
      lcd.setCursor(0, 0);
      lcd.print(titleBlink ? "Click to start  " : "                ");
      lcd.setCursor(0, 1);
      lcd.print(titleBlink ? "                " : "Hold for menu   ");
    }
    if (buttonAction == 1) {
      initRunnerGraphics();
      heroPos = 1; playing = true; coins = 0;
      newTerrainType = 0; newTerrainDuration = 1;
      gameIsActive = true;
      showTitle = false;
      gameStartTime = millis();
      lastObstacleRow = 0;
      obstacleGap = 0;
      lcd.clear();
    }
    if (buttonAction == 2) {
      showTitle = true;
      gameState = STATE_IDLE;
      lcd.clear();
    }
    return;
  }

  if (--newTerrainDuration == 0) {
    if (newTerrainType == 0) {
      if (obstacleGap > 0) {
        obstacleGap--;
        newTerrainType = (random(2) == 0) ? 3 : 4;
        newTerrainDuration = 2;
      } else {
        int r = random(10);
        if (r < 6) {
          newTerrainType = (random(2) == 0) ? 3 : 4;
          newTerrainDuration = 2;
        } else {
          if (lastObstacleRow == 1) {
            newTerrainType = 2;
            lastObstacleRow = 2;
          } else {
            newTerrainType = 1;
            lastObstacleRow = 1;
          }
          newTerrainDuration = 2;
          obstacleGap = 3;
        }
      }
    } else {
      newTerrainType = 0;
      newTerrainDuration = 3 + random(3);
    }
  }

  advanceTerrain(terrainLower, newTerrainType == 1 ? 5 : (newTerrainType == 3 ? 4 : ' '));
  advanceTerrain(terrainUpper, newTerrainType == 2 ? 5 : (newTerrainType == 4 ? 4 : ' '));
  
  if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause) {
    if (heroPos <= 2) heroPos = 3;
  }

  char upperSave = terrainUpper[HERO_HORIZONTAL_POSITION];
  char lowerSave = terrainLower[HERO_HORIZONTAL_POSITION];
  byte upper = ' ', lower = ' ';
  bool collide = false;
  bool gotCoin = false;
  
  if (heroPos <= 2) { 
    upper = ' '; 
    lower = heroPos; 
  }
  else if (heroPos == 3 || heroPos == 10) { 
    upper = ' '; 
    lower = 3; 
  }
  else if (heroPos == 4 || heroPos == 9) { 
    upper = ' '; 
    lower = 3; 
  }
  else if (heroPos >= 5 && heroPos <= 8) { 
    upper = 3; 
    lower = ' '; 
  }
  else if (heroPos >= 11) { 
    upper = (heroPos == 11) ? 1 : 2; 
    lower = ' '; 
  }
  
  if (upperSave == 4 && upper != ' ') {
    gotCoin = true;
    upperSave = ' ';
  }
  if (lowerSave == 4 && lower != ' ') {
    gotCoin = true;
    lowerSave = ' ';
  }
  
  if (gotCoin) coins++;
  
  if (upper != ' ') { 
    collide = (upperSave == 5 || upperSave == 6 || upperSave == 7);
    terrainUpper[HERO_HORIZONTAL_POSITION] = upper;
  }
  if (lower != ' ') { 
    collide |= (lowerSave == 5 || lowerSave == 6 || lowerSave == 7);
    terrainLower[HERO_HORIZONTAL_POSITION] = lower;
  }
  
  terrainUpper[TERRAIN_WIDTH] = '\0';
  terrainLower[TERRAIN_WIDTH] = '\0';
  
  static char lastUpper[TERRAIN_WIDTH + 1] = "";
  static char lastLower[TERRAIN_WIDTH + 1] = "";
  
  for (int i = 0; i < TERRAIN_WIDTH; i++) {
    if (terrainUpper[i] != lastUpper[i]) {
      lcd.setCursor(i, 0);
      if (terrainUpper[i] == ' ') lcd.print(' ');
      else lcd.write(terrainUpper[i]);
      lastUpper[i] = terrainUpper[i];
    }
  }
  
  for (int i = 0; i < TERRAIN_WIDTH; i++) {
    if (terrainLower[i] != lastLower[i]) {
      lcd.setCursor(i, 1);
      if (terrainLower[i] == ' ') lcd.print(' ');
      else lcd.write(terrainLower[i]);
      lastLower[i] = terrainLower[i];
    }
  }
  
  static unsigned int lastCoins = 9999;
  if (coins != lastCoins) {
    lcd.setCursor(12, 0);
    lcd.print(coins);
    lcd.print("  ");
    lastCoins = coins;
  }
  
  terrainUpper[HERO_HORIZONTAL_POSITION] = upperSave;
  terrainLower[HERO_HORIZONTAL_POSITION] = lowerSave;

  if (collide) {
    totalPlayTime[0] += (millis() - gameStartTime) / 1000;
    saveHighScore(0, coins);
    playing = false;
    showScore = true;
  } else {
    if (heroPos == 2 || heroPos == 10) heroPos = 1;
    else if ((heroPos >= 5 && heroPos <= 7) && (terrainLower[HERO_HORIZONTAL_POSITION] == 5 || terrainLower[HERO_HORIZONTAL_POSITION] == 6 || terrainLower[HERO_HORIZONTAL_POSITION] == 7)) heroPos = 11;
    else if (heroPos >= 11 && terrainLower[HERO_HORIZONTAL_POSITION] == ' ') heroPos = 7;
    else if (heroPos == 12) heroPos = 11;
    else ++heroPos;
  }
  
  int baseSpeed = (difficulty == 0) ? 120 : (difficulty == 1 ? 100 : 80);
  int spd = coins >= 30 ? baseSpeed - 40 : (coins >= 15 ? baseSpeed - 20 : baseSpeed);
  delay(spd);
}

// ==================== SNAKE ====================
void initSnakeGraphics() {
  static byte head[] = {B00000,B01110,B11111,B10101,B11111,B01110,B00000,B00000};
  static byte body[] = {B00000,B01110,B11111,B11111,B11111,B01110,B00000,B00000};
  static byte food[] = {B00000,B00100,B01110,B11111,B01110,B00100,B00000,B00000};
  lcd.createChar(1, head);
  lcd.createChar(2, body);
  lcd.createChar(3, food);
}

void spawnFood() {
  bool valid;
  do {
    valid = true;
    foodX = random(16);
    foodY = random(2);
    for (byte i = 0; i < snakeLen; i++) {
      if (snakeX[i] == foodX && snakeY[i] == foodY) valid = false;
    }
  } while (!valid);
}

void snakeGame() {
  static bool playing = false;
  static bool showTitle = true;
  static bool titleBlink = false;
  static unsigned long lastMove = 0;
  static unsigned long lastTitleBlink = 0;
  static unsigned int score = 0;
  static bool showScore = false;
  static unsigned long gameStartTime = 0;
  
  if (showScore) {
    gameIsActive = false;
    lcd.setCursor(0, 0);
    lcd.print("Score:");
    lcd.print(score);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print("High:");
    lcd.print(highScores[1]);
    lcd.print("         ");
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[1]++;
      showScore = false;
      showTitle = true;
      lcd.clear();
    }
    return;
  }
  
  if (showTitle) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink;
      lastTitleBlink = millis();
      lcd.setCursor(0, 0);
      lcd.print(titleBlink ? "Click to start  " : "                ");
      lcd.setCursor(0, 1);
      lcd.print(titleBlink ? "                " : "Hold for menu   ");
    }
    if (buttonAction == 1) {
      initSnakeGraphics();
      snakeLen = 3; snakeDir = 0; score = 0;
      for (byte i = 0; i < snakeLen; i++) { snakeX[i] = 5 - i; snakeY[i] = 0; }
      spawnFood();
      playing = true;
      gameIsActive = true;
      showTitle = false;
      gameStartTime = millis();
      lcd.clear();
    }
    if (buttonAction == 2) {
      showTitle = true;
      gameState = STATE_IDLE;
      lcd.clear();
    }
    return;
  }

  if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause) {
    snakeDir = (snakeDir == 0) ? 1 : 0;
    delay(150);
  }

  int baseSpeed = (difficulty == 0) ? 300 : (difficulty == 1 ? 250 : 200);
  if (millis() - lastMove < (unsigned long)(baseSpeed - score * 3)) return;
  lastMove = millis();

  byte newX = snakeX[0] + 1;
  byte newY = snakeY[0];
  if (snakeDir == 0 && newY > 0) newY--;
  else if (snakeDir == 1 && newY < 1) newY++;
  if (newX > 15) newX = 0;

  for (byte i = 1; i < snakeLen; i++) {
    if (snakeX[i] == newX && snakeY[i] == newY) {
      totalPlayTime[1] += (millis() - gameStartTime) / 1000;
      saveHighScore(1, score);
      playing = false;
      showScore = true;
      return;
    }
  }

  for (byte i = snakeLen; i > 0; i--) {
    snakeX[i] = snakeX[i-1];
    snakeY[i] = snakeY[i-1];
  }
  snakeX[0] = newX;
  snakeY[0] = newY;

  if (newX == foodX && newY == foodY) {
    snakeLen++;
    score++;
    spawnFood();
  }

  static byte lastSnakeX[32], lastSnakeY[32], lastLen = 0;
  static byte lastFoodX = 255, lastFoodY = 255;
  static unsigned int lastScore = 9999;
  
  for (byte i = 0; i < lastLen; i++) {
    bool stillOccupied = false;
    for (byte j = 0; j < snakeLen; j++) {
      if (snakeX[j] == lastSnakeX[i] && snakeY[j] == lastSnakeY[i]) {
        stillOccupied = true;
        break;
      }
    }
    if (!stillOccupied && !(lastSnakeX[i] == foodX && lastSnakeY[i] == foodY)) {
      lcd.setCursor(lastSnakeX[i], lastSnakeY[i]);
      lcd.print(' ');
    }
  }
  
  if (lastFoodX != foodX || lastFoodY != foodY) {
    lcd.setCursor(lastFoodX, lastFoodY);
    lcd.print(' ');
  }
  
  for (byte i = 0; i < snakeLen; i++) {
    lcd.setCursor(snakeX[i], snakeY[i]);
    lcd.write(i == 0 ? 1 : 2);
    lastSnakeX[i] = snakeX[i];
    lastSnakeY[i] = snakeY[i];
  }
  lastLen = snakeLen;
  
  lcd.setCursor(foodX, foodY);
  lcd.write(3);
  lastFoodX = foodX;
  lastFoodY = foodY;
  
  if (score != lastScore) {
    lcd.setCursor(14, 0);
    lcd.print(score);
    lcd.print(" ");
    lastScore = score;
  }
}

// ==================== REACTION ====================
void reactionGame() {
  static byte phase = 0;
  static unsigned long waitStart = 0, targetTime = 0;
  static unsigned int bestTime = 9999;
  static bool showScore = false;
  static bool titleBlink = false;
  static unsigned long lastTitleBlink = 0;
  static unsigned long gameStartTime = 0;
  
  if (showScore) {
    gameIsActive = false;
    lcd.setCursor(0, 0);
    lcd.print("Best:");
    lcd.print(bestTime);
    lcd.print("ms      ");
    lcd.setCursor(0, 1);
    lcd.print("Record:");
    lcd.print(highScores[2]);
    lcd.print("ms   ");
    delay(100);
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[2]++;
      showScore = false;
      phase = 0;
      lcd.clear();
    }
    return;
  }
  
  if (phase == 0) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink;
      lastTitleBlink = millis();
      lcd.setCursor(0, 0);
      lcd.print(titleBlink ? "Click to start  " : "                ");
      lcd.setCursor(0, 1);
      lcd.print(titleBlink ? "                " : "Hold for menu   ");
    }
    if (buttonAction == 1) {
      phase = 1;
      waitStart = millis();
      targetTime = random(2000, 5000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wait for it...");
      gameIsActive = true;
      gameStartTime = millis();
      delay(200);
    }
    if (buttonAction == 2) {
      gameState = STATE_IDLE;
      lcd.clear();
    }
    return;
  }
  
  if (phase == 1) {
    if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Too early!");
      gameIsActive = false;
      delay(1500);
      phase = 0;
      showScore = true;
      lcd.clear();
      return;
    }
    if (millis() - waitStart >= targetTime) {
      phase = 2;
      waitStart = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(">>> NOW! <<<");
    }
    return;
  }
  
  if (phase == 2) {
    if (digitalRead(PIN_BUTTON) == LOW) {
      unsigned int reaction = millis() - waitStart;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Time: ");
      lcd.print(reaction);
      lcd.print("ms");
      if (reaction < bestTime) bestTime = reaction;
      if (highScores[2] == 0 || reaction < highScores[2]) {
        saveHighScore(2, reaction);
        lcd.setCursor(0, 1);
        lcd.print("NEW RECORD!");
      }
      totalPlayTime[2] += (millis() - gameStartTime) / 1000;
      gameIsActive = false;
      delay(2000);
      phase = 0;
      showScore = true;
      lcd.clear();
    }
    if (millis() - waitStart > 2000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Too slow!");
      totalPlayTime[2] += (millis() - gameStartTime) / 1000;
      gameIsActive = false;
      delay(1500);
      phase = 0;
      showScore = true;
      lcd.clear();
    }
  }
}

// ==================== PONG ====================
void pongGame() {
  static byte ballX = 8, ballY = 0;
  static int ballDX = 1, ballDY = 1;
  static byte paddleY = 0;
  static byte cpuPaddleY = 1;
  static unsigned int score = 0;
  static unsigned int cpuScore = 0;
  static bool playing = false;
  static bool showTitle = true;
  static bool titleBlink = false;
  static unsigned long lastTitleBlink = 0;
  static unsigned long lastMove = 0;
  static unsigned long lastPaddleMove = 0;
  static bool showScore = false;
  static unsigned long gameStartTime = 0;
  
  if (showScore) {
    gameIsActive = false;
    lcd.setCursor(0, 0);
    if (score > cpuScore) {
      lcd.print("YOU WIN! ");
      lcd.print(score);
      lcd.print("-");
      lcd.print(cpuScore);
    } else {
      lcd.print("YOU LOSE! ");
      lcd.print(score);
      lcd.print("-");
      lcd.print(cpuScore);
    }
    lcd.setCursor(0, 1);
    lcd.print("Best:");
    lcd.print(highScores[3]);
    lcd.print("         ");
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[3]++;
      showScore = false;
      showTitle = true;
      playing = false;
      score = 0;
      cpuScore = 0;
      lcd.clear();
    }
    return;
  }
  
  if (showTitle) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink;
      lastTitleBlink = millis();
      lcd.setCursor(0, 0);
      lcd.print(titleBlink ? "PONG - First 5  " : "                ");
      lcd.setCursor(0, 1);
      lcd.print(titleBlink ? "Click to start  " : "Hold for menu   ");
    }
    if (buttonAction == 1) {
      ballX = 8; ballY = random(2);
      ballDX = random(2) == 0 ? -1 : 1;
      ballDY = random(2) == 0 ? -1 : 1;
      paddleY = 0;
      cpuPaddleY = 1;
      score = 0;
      cpuScore = 0;
      playing = true;
      gameIsActive = true;
      showTitle = false;
      gameStartTime = millis();
      lcd.clear();
    }
    if (buttonAction == 2) {
      showTitle = true;
      gameState = STATE_IDLE;
      lcd.clear();
    }
    return;
  }
  
  if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause && millis() - lastPaddleMove > 200) {
    paddleY = (paddleY == 0) ? 1 : 0;
    lastPaddleMove = millis();
  }
  
  if (millis() - lastMove < 180) return;
  lastMove = millis();
  
  if (ballX > 8 && ballDX > 0) {
    if (cpuPaddleY != ballY && random(10) < 7) {
      cpuPaddleY = ballY;
    }
  }
  
  ballX += ballDX;
  ballY += ballDY;
  
  if (ballY >= 1) { ballY = 1; ballDY = -1; }
  if (ballY <= 0) { ballY = 0; ballDY = 1; }
  
  if (ballX == 1 && ballY == paddleY && ballDX < 0) {
    ballX = 2;
    ballDX = 1;
    ballDY = random(2) == 0 ? -1 : 1;
  }
  
  if (ballX == 14 && ballY == cpuPaddleY && ballDX > 0) {
    ballX = 13;
    ballDX = -1;
    ballDY = random(2) == 0 ? -1 : 1;
  }
  
  if (ballX <= 0 && ballDX < 0) {
    cpuScore++;
    if (cpuScore >= 5) {
      totalPlayTime[3] += (millis() - gameStartTime) / 1000;
      saveHighScore(3, score);
      playing = false;
      showScore = true;
      return;
    }
    ballX = 8; ballY = random(2);
    ballDX = 1; ballDY = random(2) == 0 ? -1 : 1;
    delay(500);
  }
  
  if (ballX >= 15 && ballDX > 0) {
    score++;
    if (score >= 5) {
      totalPlayTime[3] += (millis() - gameStartTime) / 1000;
      saveHighScore(3, score);
      playing = false;
      showScore = true;
      return;
    }
    ballX = 8; ballY = random(2);
    ballDX = -1; ballDY = random(2) == 0 ? -1 : 1;
    delay(500);
  }
  
  lcd.clear();
  lcd.setCursor(0, paddleY);
  lcd.print("|");
  lcd.setCursor(15, cpuPaddleY);
  lcd.print("|");
  if (ballX >= 0 && ballX < 16 && ballY >= 0 && ballY < 2) {
    lcd.setCursor(ballX, ballY);
    lcd.print("o");
  }
  lcd.setCursor(6, 0);
  lcd.print(score);
  lcd.print("-");
  lcd.print(cpuScore);
}

// ==================== MEMORY GAME ====================
void memoryGame() {
  static byte sequence[20];
  static byte seqLen = 0;
  static byte playerPos = 0;
  static byte phase = 0;
  static unsigned int score = 0;
  static bool titleBlink = false;
  static unsigned long lastTitleBlink = 0;
  static unsigned long lastBlink = 0;
  static byte showPos = 0;
  static unsigned long gameStartTime = 0;
  static bool waitingInput = true;
  
  if (phase == 3) {
    gameIsActive = false;
    lcd.setCursor(0, 0);
    lcd.print("Level:");
    lcd.print(score);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print("Best:");
    lcd.print(highScores[4]);
    lcd.print("         ");
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[4]++;
      phase = 0;
      seqLen = 0;
      lcd.clear();
    }
    return;
  }
  
  if (phase == 0) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink;
      lastTitleBlink = millis();
      lcd.setCursor(0, 0);
      lcd.print(titleBlink ? "Memory Game     " : "                ");
      lcd.setCursor(0, 1);
      lcd.print(titleBlink ? "Click to start  " : "Hold for menu   ");
    }
    if (buttonAction == 1) {
      seqLen = 0;
      playerPos = 0;
      score = 0;
      phase = 1;
      showPos = 0;
      gameIsActive = true;
      gameStartTime = millis();
      lcd.clear();
    }
    if (buttonAction == 2) {
      gameState = STATE_IDLE;
      lcd.clear();
    }
    return;
  }
  
  if (phase == 1) {
    if (showPos == 0 && (seqLen == 0 || playerPos >= seqLen)) {
      sequence[seqLen] = random(2);
      seqLen++;
    }
    
    if (showPos < seqLen) {
      if (millis() - lastBlink > 600) {
        lcd.clear();
        lcd.setCursor(7, sequence[showPos]);
        lcd.print("**");
        delay(400);
        lcd.clear();
        showPos++;
        lastBlink = millis();
      }
    } else {
      phase = 2;
      playerPos = 0;
      waitingInput = true;
      lcd.clear();
      lcd.print("Your turn!");
      delay(800);
      lcd.clear();
    }
    return;
  }
  
  if (phase == 2) {
    lcd.setCursor(0, 0);
    lcd.print("Top or Bottom?");
    lcd.setCursor(0, 1);
    lcd.print("Done:");
    lcd.print(playerPos);
    lcd.print("/");
    lcd.print(seqLen);
    lcd.print("  ");
    
    if (buttonAction == 1 && waitingInput) {
      waitingInput = false;
      byte input = 0;
      
      if (input == sequence[playerPos]) {
        playerPos++;
        lcd.clear();
        lcd.print("Correct!");
        delay(400);
        if (playerPos >= seqLen) {
          score++;
          phase = 1;
          showPos = 0;
          lcd.clear();
          lcd.print("Level ");
          lcd.print(score + 1);
          lcd.print("!");
          delay(1000);
          lcd.clear();
        } else {
          waitingInput = true;
        }
      } else {
        totalPlayTime[4] += (millis() - gameStartTime) / 1000;
        saveHighScore(4, score);
        phase = 3;
        lcd.clear();
        lcd.print("Wrong!");
        delay(1500);
      }
    } else if (buttonAction == 2 && waitingInput) {
      waitingInput = false;
      byte input = 1;
      
      if (input == sequence[playerPos]) {
        playerPos++;
        lcd.clear();
        lcd.print("Correct!");
        delay(400);
        if (playerPos >= seqLen) {
          score++;
          phase = 1;
          showPos = 0;
          lcd.clear();
          lcd.print("Level ");
          lcd.print(score + 1);
          lcd.print("!");
          delay(1000);
          lcd.clear();
        } else {
          waitingInput = true;
        }
      } else {
        totalPlayTime[4] += (millis() - gameStartTime) / 1000;
        saveHighScore(4, score);
        phase = 3;
        lcd.clear();
        lcd.print("Wrong!");
        delay(1500);
      }
    }
    
    if (!waitingInput && buttonAction == 0) {
      waitingInput = true;
    }
  }
}

// ==================== FLAPPY BIRD ====================
void flappyGame() {
  static byte birdY = 0;
  static byte pipeX = 15;
  static byte pipeGap = 0;
  static unsigned int score = 0;
  static byte lives = 3;
  static bool playing = false;
  static bool showTitle = true;
  static bool titleBlink = false;
  static unsigned long lastTitleBlink = 0;
  static unsigned long lastMove = 0;
  static unsigned long lastFlap = 0;
  static bool showScore = false;
  static unsigned long gameStartTime = 0;
  static bool hitPipe = false;
  
  if (showScore) {
    gameIsActive = false;
    lcd.setCursor(0, 0);
    lcd.print("Score:");
    lcd.print(score);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print("Best:");
    lcd.print(highScores[5]);
    lcd.print("         ");
    if (buttonAction == 1 || buttonAction == 2) {
      gamesPlayed[5]++;
      showScore = false;
      showTitle = true;
      playing = false;
      lives = 3;
      lcd.clear();
    }
    return;
  }
  
  if (showTitle) {
    gameIsActive = false;
    if (millis() - lastTitleBlink > 500) {
      titleBlink = !titleBlink;
      lastTitleBlink = millis();
      lcd.setCursor(0, 0);
      lcd.print(titleBlink ? "Flappy - 3 Lives" : "                ");
      lcd.setCursor(0, 1);
      lcd.print(titleBlink ? "Click=Flap UP   " : "Hold for menu   ");
    }
    if (buttonAction == 1) {
      birdY = 0;
      pipeX = 15;
      pipeGap = random(2);
      score = 0;
      lives = 3;
      playing = true;
      gameIsActive = true;
      showTitle = false;
      gameStartTime = millis();
      hitPipe = false;
      lcd.clear();
    }
    if (buttonAction == 2) {
      showTitle = true;
      gameState = STATE_IDLE;
      lcd.clear();
    }
    return;
  }
  
  if (digitalRead(PIN_BUTTON) == LOW && !holdingForPause && millis() - lastFlap > 250) {
    birdY = 0;
    lastFlap = millis();
  }
  
  if (millis() - lastMove < 300) return;
  lastMove = millis();
  
  if (birdY == 0 && millis() - lastFlap > 400) {
    birdY = 1;
  }
  
  if (!hitPipe) {
    if (pipeX > 0) {
      pipeX--;
    } else {
      pipeX = 15;
      pipeGap = random(2);
      if (!hitPipe) score++;
    }
  }
  
  if (pipeX == 1 && !hitPipe) {
    if ((pipeGap == 1 && birdY == 0) || (pipeGap == 0 && birdY == 1)) {
      lives--;
      hitPipe = true;
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print("CRASH!");
      lcd.setCursor(2, 1);
      lcd.print("Lives:");
      lcd.print(lives);
      delay(1000);
      if (lives == 0) {
        totalPlayTime[5] += (millis() - gameStartTime) / 1000;
        saveHighScore(5, score);
        playing = false;
        showScore = true;
        return;
      }
      pipeX = 15;
      birdY = 0;
      hitPipe = false;
    }
  }
  
  lcd.clear();
  lcd.setCursor(1, birdY);
  lcd.print(">");
  
  if (pipeGap == 1) {
    lcd.setCursor(pipeX, 0);
    lcd.print("#");
  } else {
    lcd.setCursor(pipeX, 1);
    lcd.print("#");
  }
  
  lcd.setCursor(11, 0);
  lcd.print(score);
  lcd.setCursor(11, 1);
  for (byte i = 0; i < lives; i++) {
    lcd.print((char)3);
  }
}

// ==================== STOPWATCH ====================
void stopwatchApp() {
  static bool running = false;
  static unsigned long startTime = 0;
  static unsigned long elapsed = 0;
  static bool initialized = false;
  
  if (!initialized) {
    lcd.clear();
    initialized = true;
  }
  
  if (buttonAction == 1) {
    if (!running) {
      running = true;
      startTime = millis() - elapsed;
    } else {
      running = false;
      elapsed = millis() - startTime;
    }
  }
  
  if (buttonAction == 2) {
    if (elapsed == 0 && !running) {
      initialized = false;
      elapsed = 0;
      running = false;
      gameState = STATE_MAIN_MENU;
      lcd.clear();
      drawMainMenu();
      return;
    } else {
      elapsed = 0;
      running = false;
      lcd.clear();
      initialized = true;
    }
  }
  
  if (running) {
    elapsed = millis() - startTime;
  }
  
  unsigned long secs = elapsed / 1000;
  unsigned long mins = secs / 60;
  secs = secs % 60;
  unsigned long ms = (elapsed % 1000) / 10;
  
  lcd.setCursor(0, 0);
  lcd.print("Stopwatch");
  lcd.setCursor(2, 1);
  if (mins < 10) lcd.print("0");
  lcd.print(mins);
  lcd.print(":");
  if (secs < 10) lcd.print("0");
  lcd.print(secs);
  lcd.print(":");
  if (ms < 10) lcd.print("0");
  lcd.print(ms);
  
  delay(50);
}

// ==================== CALCULATOR ====================
void calculatorApp() {
  static long num1 = 0, num2 = 0;
  static byte operation = 0;
  static bool enteringNum2 = false;
  static bool initialized = false;
  
  if (!initialized) {
    lcd.clear();
    num1 = 0;
    num2 = 0;
    operation = 0;
    enteringNum2 = false;
    initialized = true;
  }
  
  if (buttonAction == 2 && operation == 0 && !enteringNum2) {
    initialized = false;
    gameState = STATE_MAIN_MENU;
    lcd.clear();
    drawMainMenu();
    return;
  }
  
  lcd.setCursor(0, 0);
  lcd.print("Calc [Hold=Exit]");
  lcd.setCursor(0, 1);
  if (!enteringNum2) {
    lcd.print(num1);
  } else {
    char ops[] = {' ', '+', '-', '*', '/'};
    lcd.print(num1);
    lcd.print(ops[operation]);
    lcd.print(num2);
  }
  lcd.print("        ");
  
  if (buttonAction == 1) {
    if (!enteringNum2) {
      num1++;
      if (num1 > 9999) num1 = -9999;
    } else {
      num2++;
      if (num2 > 9999) num2 = -9999;
    }
  }
  
  if (buttonAction == 2 && operation > 0) {
    if (!enteringNum2) {
      enteringNum2 = true;
      num2 = 0;
    } else {
      long result = 0;
      switch(operation) {
        case 1: result = num1 + num2; break;
        case 2: result = num1 - num2; break;
        case 3: result = num1 * num2; break;
        case 4: result = (num2 != 0) ? num1 / num2 : 0; break;
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Result:");
      lcd.setCursor(0, 1);
      lcd.print(result);
      delay(2000);
      num1 = result;
      num2 = 0;
      enteringNum2 = false;
      operation = 0;
      lcd.clear();
    }
  }
  
  if (buttonAction == 1 && !enteringNum2 && digitalRead(PIN_BUTTON) == HIGH) {
    delay(100);
    if (digitalRead(PIN_BUTTON) == HIGH) {
      operation++;
      if (operation > 4) operation = 1;
      lcd.setCursor(0, 0);
      char ops[] = {' ', '+', '-', '*', '/'};
      lcd.print("Op: ");
      lcd.print(ops[operation]);
      lcd.print("           ");
      delay(500);
    }
  }
  
  delay(100);
}

// ==================== DICE ROLLER ====================
void diceApp() {
  static byte sides = 6;
  static int result = 0;
  static bool initialized = false;
  
  if (!initialized) {
    lcd.clear();
    sides = 6;
    result = 0;
    initialized = true;
  }
  
  lcd.setCursor(0, 0);
  lcd.print("Dice: D");
  lcd.print(sides);
  lcd.print(" [H=Exit]");
  
  if (buttonAction == 1) {
    sides += 2;
    if (sides > 20) sides = 4;
    result = 0;
  }
  
  if (buttonAction == 2) {
    if (result == 0) {
      initialized = false;
      gameState = STATE_MAIN_MENU;
      lcd.clear();
      drawMainMenu();
      return;
    } else {
      for (int i = 0; i < 10; i++) {
        result = random(1, sides + 1);
        lcd.setCursor(0, 1);
        lcd.print("Rolling: ");
        lcd.print(result);
        lcd.print("    ");
        delay(100);
      }
    }
  }
  
  lcd.setCursor(0, 1);
  if (result > 0) {
    lcd.print("Result: ");
    lcd.print(result);
    lcd.print("    ");
  } else {
    lcd.print("Hold to roll    ");
  }
  
  delay(100);
}

// ==================== SCREENSAVER ====================
void screensaver() {
  static byte x = 0, y = 0;
  static int dx = 1, dy = 1;
  static unsigned long lastMove = 0;
  static bool initialized = false;
  
  if (!initialized) {
    lcd.clear();
    x = random(16);
    y = random(2);
    initialized = true;
  }
  
  if (millis() - lastMove > 200) {
    lcd.setCursor(x, y);
    lcd.print(" ");
    
    x += dx;
    y += dy;
    
    if (x >= 15) { x = 15; dx = -1; }
    if (x == 0) dx = 1;
    if (y >= 1) { y = 1; dy = -1; }
    if (y == 0) dy = 1;
    
    lcd.setCursor(x, y);
    lcd.print("*");
    lastMove = millis();
  }
  
  if (buttonAction == 1 || buttonAction == 2) {
    initialized = false;
    gameState = STATE_IDLE;
    lcd.clear();
  }
}

// ==================== STATS ====================
void statsApp() {
  static byte page = 0;
  static bool initialized = false;
  
  if (!initialized) {
    lcd.clear();
    page = 0;
    initialized = true;
  }
  
  if (buttonAction == 1) {
    page = (page + 1) % 6;
    lcd.clear();
  }
  
  if (buttonAction == 2) {
    initialized = false;
    gameState = STATE_MAIN_MENU;
    lcd.clear();
    drawMainMenu();
    return;
  }
  
  lcd.setCursor(0, 0);
  lcd.print(gameNames[page]);
  lcd.print(" [H=Exit] ");
  
  lcd.setCursor(0, 1);
  lcd.print("Hi:");
  lcd.print(highScores[page]);
  lcd.print(" P:");
  lcd.print(gamesPlayed[page]);
  lcd.print("    ");
  
  delay(100);
}

// ==================== SETTINGS ====================
void settingsApp() {
  static byte option = 0;
  static bool initialized = false;
  
  if (!initialized) {
    lcd.clear();
    option = 0;
    initialized = true;
  }
  
  if (buttonAction == 1) {
    if (option == 0) {
      difficulty = (difficulty + 1) % 3;
      EEPROM.write(20, difficulty);
    } else if (option == 1) {
      for (int i = 0; i < 6; i++) {
        highScores[i] = 0;
        gamesPlayed[i] = 0;
        EEPROM.write(i * 2, 0);
        EEPROM.write(i * 2 + 1, 0);
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Scores reset!");
      delay(1000);
      lcd.clear();
    }
  }
  
  if (buttonAction == 2) {
    option = (option + 1) % 3;
    if (option == 2) {
      initialized = false;
      gameState = STATE_MAIN_MENU;
      lcd.clear();
      drawMainMenu();
      return;
    }
    lcd.clear();
  }
  
  lcd.setCursor(0, 0);
  if (option == 0) {
    lcd.print(">Difficulty     ");
    lcd.setCursor(0, 1);
    if (difficulty == 0) lcd.print("Easy            ");
    else if (difficulty == 1) lcd.print("Normal          ");
    else lcd.print("Hard            ");
  } else if (option == 1) {
    lcd.print(">Reset Stats    ");
    lcd.setCursor(0, 1);
    lcd.print("Click to reset  ");
  }
  
  delay(100);
}

// ==================== SETUP & LOOP ====================
void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  bluetooth.begin(9600);
  lcd.init();
  lcd.backlight();
  loadHighScores();
  randomSeed(analogRead(0));
}

void loop() {
  updateButton();
  switch (gameState) {
    case STATE_BOOT: slotMachineBoot(); break;
    case STATE_IDLE: updateIdle(); break;
    case STATE_MAIN_MENU: updateMainMenu(); break;
    case STATE_GAMES_MENU: updateGamesMenu(); break;
    case STATE_RUNNER: runnerGame(); break;
    case STATE_SNAKE: snakeGame(); break;
    case STATE_REACTION: reactionGame(); break;
    case STATE_PONG: pongGame(); break;
    case STATE_MEMORY: memoryGame(); break;
    case STATE_FLAPPY: flappyGame(); break;
    case STATE_SETTINGS: settingsApp(); break;
    case STATE_STOPWATCH: stopwatchApp(); break;
    case STATE_CALCULATOR: calculatorApp(); break;
    case STATE_DICE: diceApp(); break;
    case STATE_SCREENSAVER: screensaver(); break;
    case STATE_STATS: statsApp(); break;
    case STATE_PAUSED: updatePause(); break;
    case STATE_WEATHER: weatherApp(); break;
  }
}
