/*
  =========================================================
   LUMI - Study Buddy Lamp
  =========================================================
  What it does:
   - Shows cute animated eyes when nobody is studying.
   - When you sit within 40 cm for 5 seconds, the study timer starts.
   - If you leave for 10 seconds, the session ends and is added to today's total.
   - Today's total is saved, so it survives unplugging.

  Extra features:
   - Daily goal with a progress bar (default 3 hours) + celebration when reached
   - Break reminder every 50 minutes
   - Countdown screen when you step away ("Where'd you go?")
   - Short button press = stats screen, hold 3 seconds = new day reset
   - Sends live data to the browser page over USB (one JSON line per second)
   - Browser page can send commands: GOAL 120 / RESET / MUTE / UNMUTE / BEEP

  Wiring (Arduino Uno):
   HC-SR04  VCC -> 5V   GND -> GND   TRIG -> D9   ECHO -> D10
   OLED     VCC -> 5V   GND -> GND   SDA  -> A4   SCL  -> A5
   Buzzer   +   -> D8   -   -> GND
   Button   one leg -> D2, the diagonal leg -> GND (no resistor needed)

  Libraries to install (Arduino IDE -> Tools -> Manage Libraries):
   - Adafruit SSD1306
   - Adafruit GFX Library
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// ---------- Pins ----------
#define TRIG_PIN    9
#define ECHO_PIN    10
#define BUZZER_PIN  8
#define BUTTON_PIN  2

// ---------- Settings you can change ----------
#define SIT_DISTANCE_CM   40        // closer than this = you are at the desk
#define START_DELAY_MS    5000UL    // stay this long before the timer starts
#define STOP_DELAY_MS     10000UL   // gone this long = session ends
#define BREAK_EVERY_MIN   50        // break reminder interval
#define DEFAULT_GOAL_MIN  180       // daily goal in minutes
#define SAVE_EVERY_MS     60000UL   // save progress every minute while studying
#define RESET_HOLD_MS     3000UL    // hold button this long for a new day

// ---------- Screen ----------
#define SCREEN_W 128
#define SCREEN_H 64
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ---------- Saved data (EEPROM) ----------
struct SaveData {
  uint16_t magic;
  uint32_t todaySec;
  uint16_t sessions;
  uint16_t goalMin;
  uint32_t bestSec;
  uint8_t  muted;
};
#define MAGIC 0x4C56
SaveData data;

// ---------- Study state ----------
enum State : uint8_t { IDLE, NOTICING, STUDYING, AWAY };
State state = IDLE;

unsigned long noticeStart = 0;
unsigned long awayStart   = 0;
unsigned long lastTick    = 0;
unsigned long lastSave    = 0;
unsigned long lastSensor  = 0;
unsigned long lastStatus  = 0;
unsigned long lastFrame   = 0;
uint32_t sessionSec    = 0;
uint32_t sinceBreakSec = 0;
bool goalDone = false;

// ---------- Sensor ----------
int distanceCm = 999;
int samples[3] = {999, 999, 999};
uint8_t sampleIdx = 0;

// ---------- Messages and stats screen ----------
char msgLine1[17];
char msgLine2[17];
unsigned long msgUntil   = 0;
unsigned long statsUntil = 0;

// ---------- Button ----------
bool btnWasDown = false;
bool resetDone  = false;
unsigned long btnDownAt = 0;

// ---------- Serial commands ----------
char cmdBuf[20];
uint8_t cmdLen = 0;

// ---------- Eyes ----------
enum Mood : uint8_t { NORMAL, HAPPY, SLEEPY, SURPRISED, WINK };
Mood mood = NORMAL;
unsigned long moodUntil = 0;
unsigned long nextBlink = 0;
unsigned long blinkEnd  = 0;
unsigned long nextLook  = 0;
int lookX = 0, lookY = 0;
int targetX = 0, targetY = 0;

// =========================================================
//  Helpers
// =========================================================
void beep(uint8_t times, uint16_t ms) {
  if (data.muted) return;
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(ms);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(ms);
  }
}

void saveData() {
  EEPROM.put(0, data);
}

void loadData() {
  EEPROM.get(0, data);
  if (data.magic != MAGIC) {          // first time ever: set defaults
    data.magic = MAGIC;
    data.todaySec = 0;
    data.sessions = 0;
    data.goalMin = DEFAULT_GOAL_MIN;
    data.bestSec = 0;
    data.muted = 0;
    saveData();
  }
}

uint32_t goalSec() {
  return (uint32_t)data.goalMin * 60UL;
}

void formatHMS(uint32_t s, char* out) {          // 12:34 or 1:23:45
  unsigned h = s / 3600;
  unsigned m = (s / 60) % 60;
  unsigned sec = s % 60;
  if (h > 0) sprintf_P(out, PSTR("%u:%02u:%02u"), h, m, sec);
  else       sprintf_P(out, PSTR("%02u:%02u"), m, sec);
}

void formatHM(uint32_t s, char* out) {           // 1h 23m
  sprintf_P(out, PSTR("%uh %02um"), (unsigned)(s / 3600), (unsigned)((s / 60) % 60));
}

void printCentered(const char* s, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - (int)w) / 2, y);
  display.print(s);
}

// Text messages live in flash memory (F("...")) to save the Uno's small RAM
void printCenteredF(const __FlashStringHelper* s, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - (int)w) / 2, y);
  display.print(s);
}

void showMessage(const __FlashStringHelper* line1, const __FlashStringHelper* line2, unsigned long ms) {
  strncpy_P(msgLine1, (const char*)line1, 16); msgLine1[16] = 0;
  strncpy_P(msgLine2, (const char*)line2, 16); msgLine2[16] = 0;
  msgUntil = millis() + ms;
}

void sendEvent(const __FlashStringHelper* name) {
  Serial.print(F("{\"event\":\""));
  Serial.print(name);
  Serial.println(F("\"}"));
}

// =========================================================
//  Sensor
// =========================================================
int readDistanceOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long d = pulseIn(ECHO_PIN, HIGH, 6000UL);  // ~1 m max, keeps it fast
  if (d == 0) return 999;                              // nothing in range
  return d / 58;
}

void updateSensor() {
  if (millis() - lastSensor < 100) return;
  lastSensor = millis();
  samples[sampleIdx] = readDistanceOnce();
  sampleIdx = (sampleIdx + 1) % 3;
  int a = samples[0], b = samples[1], c = samples[2];
  distanceCm = max(min(a, b), min(max(a, b), c));     // middle value of last 3 = less jumpy
}

bool someoneThere() {
  return distanceCm > 2 && distanceCm <= SIT_DISTANCE_CM;
}

// =========================================================
//  Study sessions
// =========================================================
void celebrate() {
  showMessage(F("Goal met!"), F("Amazing work :)"), 5000);
  mood = HAPPY;
  moodUntil = millis() + 5000;
  beep(3, 80);
  beep(1, 400);
  sendEvent(F("goal"));
}

void startSession() {
  unsigned long now = millis();
  state = STUDYING;
  sessionSec = 0;
  lastTick = now;
  lastSave = now;
  data.sessions++;
  saveData();
  showMessage(F("Let's go!"), F("Timer started"), 1500);
  beep(2, 60);
  sendEvent(F("start"));
}

void endSession() {
  state = IDLE;
  saveData();
  char t[12];
  formatHMS(sessionSec, t);
  showMessage(F("Nice work!"), F(""), 3000);
  snprintf_P(msgLine2, sizeof(msgLine2), PSTR("Studied %s"), t);
  mood = HAPPY;
  moodUntil = millis() + 3000;
  sinceBreakSec = 0;
  beep(1, 300);
  sendEvent(F("end"));
}

void newDay() {
  if (state == STUDYING || state == AWAY) state = IDLE;
  data.todaySec = 0;
  data.sessions = 0;
  sessionSec = 0;
  sinceBreakSec = 0;
  goalDone = false;
  saveData();
  showMessage(F("New day!"), F("Counters reset"), 2500);
  beep(2, 150);
  sendEvent(F("newday"));
}

void countTime(unsigned long now) {
  while (now - lastTick >= 1000) {
    lastTick += 1000;
    sessionSec++;
    data.todaySec++;
    sinceBreakSec++;
    if (sessionSec > data.bestSec) data.bestSec = sessionSec;

    if (!goalDone && data.todaySec >= goalSec()) {
      goalDone = true;
      celebrate();
    }
    if (sinceBreakSec >= (uint32_t)BREAK_EVERY_MIN * 60UL) {
      sinceBreakSec = 0;
      showMessage(F("Break time!"), F("Stretch 5 min"), 6000);
      beep(3, 120);
      sendEvent(F("break"));
    }
  }
  if (now - lastSave >= SAVE_EVERY_MS) {
    saveData();
    lastSave = now;
  }
}

void updateState() {
  unsigned long now = millis();
  bool here = someoneThere();

  switch (state) {
    case IDLE:
      if (here) {
        state = NOTICING;
        noticeStart = now;
      }
      break;

    case NOTICING:                       // someone is close: wait 5 seconds
      if (!here) {
        state = IDLE;                    // just walking past
        mood = NORMAL;
        moodUntil = now + 2000;
      } else if (now - noticeStart >= START_DELAY_MS) {
        startSession();
      }
      break;

    case STUDYING:
      countTime(now);
      if (!here) {
        state = AWAY;
        awayStart = now;
      }
      break;

    case AWAY:                           // paused: come back within 10 s
      if (here) {
        state = STUDYING;
        lastTick = now;
      } else if (now - awayStart >= STOP_DELAY_MS) {
        endSession();
      }
      break;
  }
}

// =========================================================
//  Eyes
// =========================================================
void pickNextMood() {
  unsigned long now = millis();
  if (mood == SLEEPY) {                  // wake up startled
    mood = SURPRISED;
    moodUntil = now + 1500;
    return;
  }
  int r = random(100);
  if (r < 55)      { mood = NORMAL;    moodUntil = now + random(4000, 8000); }
  else if (r < 70) { mood = HAPPY;     moodUntil = now + random(2000, 4000); }
  else if (r < 80) { mood = WINK;      moodUntil = now + 700; }
  else if (r < 90) { mood = SURPRISED; moodUntil = now + random(1000, 2000); }
  else             { mood = SLEEPY;    moodUntil = now + random(8000, 30000); } // never over 30 s
}

void updateEyes() {
  unsigned long now = millis();

  if (state == NOTICING) {               // look straight at the person
    mood = SURPRISED;
    targetX = 0;
    targetY = 0;
  } else {
    if (now >= moodUntil) pickNextMood();
    if (now >= nextLook) {
      targetX = random(-8, 9);
      targetY = random(-6, 7);
      if (random(4) == 0) { targetX = 0; targetY = 0; }
      nextLook = now + random(800, 2800);
    }
  }

  if (now >= nextBlink && mood != SLEEPY && mood != HAPPY) {
    blinkEnd = now + 140;
    nextBlink = now + random(2000, 5500);
  }

  lookX += constrain(targetX - lookX, -2, 2);   // smooth movement
  lookY += constrain(targetY - lookY, -2, 2);
}

void drawEye(int cx, int cy, int w, int h, int pr) {
  display.fillRoundRect(cx - w / 2, cy - h / 2, w, h, 8, SSD1306_WHITE);
  display.fillCircle(cx + lookX, cy + lookY, pr, SSD1306_BLACK);
}

void drawEyes(int cy) {
  const int lx = 38, rx = 90;
  bool blinking = millis() < blinkEnd;

  if (mood == SLEEPY) {
    display.fillRoundRect(lx - 17, cy + 4, 34, 6, 3, SSD1306_WHITE);
    display.fillRoundRect(rx - 17, cy + 4, 34, 6, 3, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    int z = (millis() / 400) % 3;
    display.setCursor(108, 14 - z * 4);
    display.print(F("z"));
    if (z > 0) { display.setCursor(116, 8 - z * 3); display.print(F("Z")); }
    return;
  }

  if (blinking) {
    display.fillRect(lx - 17, cy - 2, 34, 4, SSD1306_WHITE);
    display.fillRect(rx - 17, cy - 2, 34, 4, SSD1306_WHITE);
    return;
  }

  if (mood == HAPPY) {                   // ^ ^ eyes
    display.fillRoundRect(lx - 17, cy - 14, 34, 28, 12, SSD1306_WHITE);
    display.fillRoundRect(rx - 17, cy - 14, 34, 28, 12, SSD1306_WHITE);
    display.fillCircle(lx, cy + 16, 21, SSD1306_BLACK);
    display.fillCircle(rx, cy + 16, 21, SSD1306_BLACK);
    return;
  }

  int w = 34, h = 34, pr = 7;
  if (mood == SURPRISED) { w = 36; h = 40; pr = 5; }

  drawEye(lx, cy, w, h, pr);
  if (mood == WINK) display.fillRect(rx - 17, cy - 2, 34, 4, SSD1306_WHITE);
  else              drawEye(rx, cy, w, h, pr);
}

// =========================================================
//  Screens
// =========================================================
void drawNoticing() {
  drawEyes(26);
  unsigned long held = millis() - noticeStart;
  if (held > START_DELAY_MS) held = START_DELAY_MS;
  int fill = (int)(held * 120UL / START_DELAY_MS);
  display.drawRect(3, 56, 122, 7, SSD1306_WHITE);
  display.fillRect(4, 57, fill, 5, SSD1306_WHITE);
}

void drawStudy() {
  char buf[12];
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Studying"));
  if ((millis() / 500) % 2) display.fillCircle(122, 3, 3, SSD1306_WHITE);

  formatHMS(sessionSec, buf);
  display.setTextSize(sessionSec >= 3600 ? 2 : 3);
  printCentered(buf, 16);

  display.setTextSize(1);
  formatHM(data.todaySec, buf);
  display.setCursor(0, 46);
  display.print(F("Today "));
  display.print(buf);

  uint32_t g = goalSec();
  uint8_t pct = 0;
  if (g > 0) pct = (uint8_t)min(100UL, data.todaySec * 100UL / g);
  display.drawRect(0, 56, 100, 8, SSD1306_WHITE);
  display.fillRect(2, 58, pct * 96 / 100, 4, SSD1306_WHITE);
  display.setCursor(104, 56);
  display.print(pct);
  display.print('%');
}

void drawAway() {
  unsigned long gone = millis() - awayStart;
  unsigned long left = gone >= STOP_DELAY_MS ? 0 : (STOP_DELAY_MS - gone + 999) / 1000;
  char buf[4];
  sprintf_P(buf, PSTR("%lu"), left);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  printCenteredF(F("Where'd you go?"), 4);
  display.setTextSize(3);
  printCentered(buf, 22);
  display.setTextSize(1);
  printCenteredF(F("Timer paused"), 54);
}

void drawMessage() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(strlen(msgLine1) <= 10 ? 2 : 1);
  printCentered(msgLine1, 16);
  display.setTextSize(1);
  printCentered(msgLine2, 44);
}

void drawStats() {
  char buf[12];
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("--- Lumi stats ---"));
  formatHM(data.todaySec, buf);
  display.setCursor(0, 14); display.print(F("Today:     ")); display.print(buf);
  display.setCursor(0, 26); display.print(F("Sessions:  ")); display.print(data.sessions);
  formatHM(data.bestSec, buf);
  display.setCursor(0, 38); display.print(F("Best ever: ")); display.print(buf);
  display.setCursor(0, 50); display.print(F("Goal:      ")); display.print(data.goalMin); display.print(F(" min"));
}

void render() {
  unsigned long now = millis();
  if (now - lastFrame < 40) return;      // about 25 frames per second
  lastFrame = now;

  display.clearDisplay();
  if (now < statsUntil)    drawStats();
  else if (now < msgUntil) drawMessage();
  else {
    switch (state) {
      case IDLE:     drawEyes(32);   break;
      case NOTICING: drawNoticing(); break;
      case STUDYING: drawStudy();    break;
      case AWAY:     drawAway();     break;
    }
  }
  display.display();
}

// =========================================================
//  Button
// =========================================================
void updateButton() {
  bool down = digitalRead(BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (down && !btnWasDown) {             // just pressed
    btnDownAt = now;
    resetDone = false;
  }
  if (down && !resetDone && now - btnDownAt >= RESET_HOLD_MS) {
    newDay();                            // held 3 seconds
    resetDone = true;
  }
  if (!down && btnWasDown && !resetDone && now - btnDownAt > 50) {
    statsUntil = now + 4000;             // short press
  }
  btnWasDown = down;
}

// =========================================================
//  Browser page (USB serial)
// =========================================================
void sendStatus() {
  if (millis() - lastStatus < 1000) return;
  lastStatus = millis();
  Serial.print(F("{\"state\":\""));
  switch (state) {
    case IDLE:     Serial.print(F("idle"));     break;
    case NOTICING: Serial.print(F("noticing")); break;
    case STUDYING: Serial.print(F("studying")); break;
    case AWAY:     Serial.print(F("away"));     break;
  }
  Serial.print(F("\",\"today\":"));    Serial.print(data.todaySec);
  Serial.print(F(",\"session\":"));    Serial.print(sessionSec);
  Serial.print(F(",\"sessions\":"));   Serial.print(data.sessions);
  Serial.print(F(",\"best\":"));       Serial.print(data.bestSec);
  Serial.print(F(",\"goal\":"));       Serial.print(data.goalMin);
  Serial.print(F(",\"dist\":"));       Serial.print(distanceCm);
  Serial.print(F(",\"muted\":"));      Serial.print(data.muted);
  Serial.println('}');
}

void handleCommand(char* c) {
  if (strncmp_P(c, PSTR("GOAL "), 5) == 0) {
    int m = atoi(c + 5);
    if (m > 0 && m <= 1440) {
      data.goalMin = m;
      goalDone = data.todaySec >= goalSec();
      saveData();
      sendEvent(F("goal_set"));
    }
  } else if (strcmp_P(c, PSTR("RESET")) == 0) {
    newDay();
  } else if (strcmp_P(c, PSTR("MUTE")) == 0) {
    data.muted = 1;
    saveData();
  } else if (strcmp_P(c, PSTR("UNMUTE")) == 0) {
    data.muted = 0;
    saveData();
    beep(1, 60);
  } else if (strcmp_P(c, PSTR("BEEP")) == 0) {
    beep(1, 100);
  }
}

void readCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = 0;
        handleCommand(cmdBuf);
        cmdLen = 0;
      }
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }
}

// =========================================================
//  Setup and loop
// =========================================================
void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  randomSeed(analogRead(A0));

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("{\"error\":\"OLED not found - check wiring\"}"));
    while (true) {                       // blink the board LED so you know
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(300);
    }
  }

  loadData();
  goalDone = data.todaySec >= goalSec();

  showMessage(F("Hi! I'm Lumi"), F("Sit to study"), 2500);
  beep(1, 80);
  nextBlink = millis() + 3000;
  moodUntil = millis() + 4000;
}

void loop() {
  readCommands();
  updateButton();
  updateSensor();
  updateState();
  updateEyes();
  render();
  sendStatus();
}
