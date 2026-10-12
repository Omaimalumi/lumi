/*
  =========================================================
   LUMI - Study Buddy Lamp
  =========================================================
  How it works:
   - Eyes on the screen when nobody is there.
   - Sit within 40 cm for 5 seconds -> the focus timer starts and COUNTS DOWN
     from the minutes you pick on the web page (for example 40:00 -> 00:00).
   - Step away: the timer pauses at once. After 10 seconds away the lamp
     switches to "Paused". Sit back down for 5 seconds to carry on.
   - Away for 5 minutes while paused -> that session is cancelled.
   - Countdown reaches 00:00 -> "Done!", then a break countdown starts.
   - Button: short press = today's stats, hold 3 seconds = new day.

  Talks to the web page (web/index.html) over USB at 115200 baud:
   Lamp -> page: S,mode,focusDone,focusMin,breakMin,modeSec,dist,pomos,today,here,overlay,pending
                 E,DONE  (when a focus session finishes)
   Page -> lamp: A<sec> ack | K<task> | F<min> focus | B<min> break | M<note> | T tune
   Modes: 0 away, 1 paused, 2 asleep, 3 saying hi, 4 focus, 5 finished, 6 break

  Wiring (Arduino Uno):
   HC-SR04  VCC -> 5V   GND -> GND   TRIG -> D9   ECHO -> D10
   OLED     VCC -> 5V   GND -> GND   SDA  -> A4   SCL  -> A5
   Buzzer   +   -> D8   -   -> GND
   Button   one leg -> D2, the diagonal leg -> GND

  Libraries (Arduino IDE -> Tools -> Manage Libraries):
   Adafruit SSD1306, Adafruit GFX Library
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

// ---------- Settings ----------
#define SIT_DISTANCE_CM  40       // closer than this = you are at the desk
#define START_DELAY_MS   5000UL   // sit this long before the timer starts or resumes
#define AWAY_GRACE_MS    10000UL  // away this long = lamp switches to "Paused"
#define GIVE_UP_SEC      300      // paused this long = session cancelled
#define DONE_SHOW_SEC    3        // how long "Done!" shows before the break
#define SAVE_EVERY_MS    60000UL  // save today's total every minute
#define RESET_HOLD_MS    3000UL   // hold the button this long for a new day

// ---------- Screen ----------
#define SCREEN_W 128
#define SCREEN_H 64
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ---------- Saved data (EEPROM) ----------
struct SaveData {
  uint16_t magic;
  uint32_t todaySec;
  uint16_t pomos;
  uint16_t focusMin;
  uint8_t  breakMin;
};
#define MAGIC 0x4C57
SaveData data;

// ---------- Modes (same numbers the web page uses) ----------
enum Mode : uint8_t { M_AWAY = 0, M_PAUSED = 1, M_ASLEEP = 2, M_HI = 3, M_FOCUS = 4, M_DONE = 5, M_BREAK = 6 };
Mode phase = M_AWAY;

unsigned long phaseStart   = 0;
unsigned long presentSince = 0;   // when you sat back down while paused
unsigned long awayStart    = 0;   // when you stepped away during focus (0 = you're here)
unsigned long lastTick     = 0;
unsigned long lastSave     = 0;
unsigned long lastSensor   = 0;
unsigned long lastStatus   = 0;
unsigned long lastFrame    = 0;
uint32_t focusDone = 0;           // seconds studied in this session
uint32_t pending   = 0;           // seconds the web page hasn't saved yet

// ---------- Sensor ----------
int distanceCm = 999;
int samples[3] = {999, 999, 999};
uint8_t sampleIdx = 0;

// ---------- Text from the web page ----------
char task[22] = "";
char note[22] = "";
unsigned long noteUntil  = 0;
unsigned long statsUntil = 0;

// ---------- Button ----------
bool btnWasDown = false;
bool resetDone  = false;
unsigned long btnDownAt = 0;

// ---------- Serial commands ----------
char cmdBuf[26];
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
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(ms);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(ms);
  }
}

void tune() {
  beep(2, 70);
  delay(120);
  beep(1, 250);
}

void saveData() {
  EEPROM.put(0, data);
}

void loadData() {
  EEPROM.get(0, data);
  if (data.magic != MAGIC) {            // first run: defaults
    data.magic = MAGIC;
    data.todaySec = 0;
    data.pomos = 0;
    data.focusMin = 25;
    data.breakMin = 5;
    saveData();
  }
}

uint32_t focusLeft() {
  uint32_t total = (uint32_t)data.focusMin * 60UL;
  return focusDone >= total ? 0 : total - focusDone;
}

uint32_t modeSec() {
  return (millis() - phaseStart) / 1000UL;
}

void formatHMS(uint32_t s, char* out) {           // 40:00 or 1:30:00
  unsigned h = s / 3600;
  unsigned m = (s / 60) % 60;
  unsigned sec = s % 60;
  if (h > 0) sprintf_P(out, PSTR("%u:%02u:%02u"), h, m, sec);
  else       sprintf_P(out, PSTR("%02u:%02u"), m, sec);
}

void formatHM(uint32_t s, char* out) {            // 1h 23m
  sprintf_P(out, PSTR("%uh %02um"), (unsigned)(s / 3600), (unsigned)((s / 60) % 60));
}

void printCentered(const char* s, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - (int)w) / 2, y);
  display.print(s);
}

void printCenteredF(const __FlashStringHelper* s, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - (int)w) / 2, y);
  display.print(s);
}

void bigTime(uint32_t s, int y) {                 // big clock, smaller if over an hour
  char buf[12];
  formatHMS(s, buf);
  display.setTextSize(s >= 3600 ? 2 : 3);
  printCentered(buf, y);
  display.setTextSize(1);
}

void fillBar(int y, uint32_t done, uint32_t total) {
  display.drawRect(3, y, 122, 7, SSD1306_WHITE);
  if (total == 0) return;
  if (done > total) done = total;
  display.fillRect(4, y + 1, (int)(done * 120UL / total), 5, SSD1306_WHITE);
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
  unsigned long d = pulseIn(ECHO_PIN, HIGH, 6000UL);   // about 1 m max
  if (d == 0) return 999;
  return d / 58;
}

void updateSensor() {
  if (millis() - lastSensor < 100) return;
  lastSensor = millis();
  samples[sampleIdx] = readDistanceOnce();
  sampleIdx = (sampleIdx + 1) % 3;
  int a = samples[0], b = samples[1], c = samples[2];
  distanceCm = max(min(a, b), min(max(a, b), c));      // middle of last 3 readings
}

bool someoneThere() {
  return distanceCm > 2 && distanceCm <= SIT_DISTANCE_CM;
}

// =========================================================
//  Web page messages
// =========================================================
uint8_t reportedMode() {
  if (phase == M_AWAY && mood == SLEEPY) return M_ASLEEP;
  return phase;
}

void sendStatus() {
  lastStatus = millis();
  unsigned long now = millis();
  uint8_t overlay = now < statsUntil ? 1 : (now < noteUntil ? 2 : 0);
  Serial.print(F("S,"));
  Serial.print(reportedMode());         Serial.print(',');
  Serial.print(focusDone);              Serial.print(',');
  Serial.print(data.focusMin);          Serial.print(',');
  Serial.print(data.breakMin);          Serial.print(',');
  Serial.print(modeSec());              Serial.print(',');
  Serial.print(distanceCm >= 999 ? 0 : distanceCm); Serial.print(',');
  Serial.print(data.pomos);             Serial.print(',');
  Serial.print(data.todaySec);          Serial.print(',');
  Serial.print(someoneThere() ? 1 : 0); Serial.print(',');
  Serial.print(overlay);                Serial.print(',');
  Serial.println(pending);
}

void copyText(char* dest, const char* src) {
  strncpy(dest, src, 21);
  dest[21] = 0;
}

void handleCommand(char* c) {
  char kind = c[0];
  char* rest = c + 1;
  if (kind == 'A') {                    // page saved some seconds
    uint32_t n = strtoul(rest, NULL, 10);
    pending = n >= pending ? 0 : pending - n;
  } else if (kind == 'K') {             // current task
    copyText(task, rest);
  } else if (kind == 'F') {             // focus length in minutes
    int n = atoi(rest);
    if (n >= 1 && n <= 600) { data.focusMin = n; saveData(); }
  } else if (kind == 'B') {             // break length in minutes
    int n = atoi(rest);
    if (n >= 1 && n <= 59) { data.breakMin = n; saveData(); }
  } else if (kind == 'M') {             // note to show on the lamp
    copyText(note, rest);
    noteUntil = millis() + 4000;
  } else if (kind == 'T') {             // play a tune
    strcpy_P(note, PSTR("La la la!"));
    noteUntil = millis() + 2500;
    tune();
  }
  sendStatus();
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
//  Study logic
// =========================================================
void setPhase(Mode p) {
  phase = p;
  phaseStart = millis();
  presentSince = 0;
  sendStatus();
}

void startFocus() {
  focusDone = 0;
  awayStart = 0;
  lastTick = millis();
  lastSave = millis();
  setPhase(M_FOCUS);
  beep(2, 60);
}

void finishFocus() {
  data.pomos++;
  saveData();
  Serial.println(F("E,DONE"));
  mood = HAPPY;
  moodUntil = millis() + 4000;
  setPhase(M_DONE);
  tune();
}

void newDay() {
  data.todaySec = 0;
  data.pomos = 0;
  focusDone = 0;
  saveData();
  strcpy_P(note, PSTR("New day!"));
  noteUntil = millis() + 2500;
  setPhase(M_AWAY);
  beep(2, 150);
}

void updateLogic() {
  unsigned long now = millis();
  bool here = someoneThere();

  switch (phase) {
    case M_AWAY:
    case M_ASLEEP:
      if (here) setPhase(M_HI);                      // someone came close
      break;

    case M_HI:                                       // wait 5 seconds to be sure you sat down
      if (!here) setPhase(M_AWAY);                   // just walking past
      else if (now - phaseStart >= START_DELAY_MS) startFocus();
      break;

    case M_FOCUS:
      if (here) {
        if (awayStart) { awayStart = 0; lastTick = now; }   // back within 10 s
        while (now - lastTick >= 1000) {             // count down one second
          lastTick += 1000;
          focusDone++;
          data.todaySec++;
          pending++;
          if (focusLeft() == 0) { finishFocus(); break; }
        }
      } else if (!awayStart) {
        awayStart = now;                             // stepped away: timer stops
      } else if (now - awayStart >= AWAY_GRACE_MS) {
        awayStart = 0;
        saveData();
        setPhase(M_PAUSED);
        beep(1, 200);
      }
      if (phase == M_FOCUS && now - lastSave >= SAVE_EVERY_MS) {
        saveData();
        lastSave = now;
      }
      break;

    case M_PAUSED:
      if (here) {
        if (!presentSince) presentSince = now;
        else if (now - presentSince >= START_DELAY_MS) {   // sat back down for 5 s
          awayStart = 0;
          lastTick = now;
          lastSave = now;
          setPhase(M_FOCUS);
          beep(1, 60);
        }
      } else {
        presentSince = 0;
        if (modeSec() >= GIVE_UP_SEC) {              // gone too long: cancel session
          focusDone = 0;
          setPhase(M_AWAY);
        }
      }
      break;

    case M_DONE:
      if (modeSec() >= DONE_SHOW_SEC) setPhase(M_BREAK);
      break;

    case M_BREAK:
      if (modeSec() >= (uint32_t)data.breakMin * 60UL) {
        focusDone = 0;
        setPhase(M_AWAY);
        beep(2, 120);                                // break over
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
  if (phase == M_HI) {                   // look straight at you
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
  lookX += constrain(targetX - lookX, -2, 2);
  lookY += constrain(targetY - lookY, -2, 2);
}

void drawEye(int cx, int cy, int w, int h, int pr) {
  display.fillRoundRect(cx - w / 2, cy - h / 2, w, h, 8, SSD1306_WHITE);
  display.fillCircle(cx + lookX, cy + lookY, pr, SSD1306_BLACK);
}

void drawEyes(int cy) {
  const int lx = 38, rx = 90;

  if (mood == SLEEPY) {
    display.fillRoundRect(lx - 17, cy + 4, 34, 6, 3, SSD1306_WHITE);
    display.fillRoundRect(rx - 17, cy + 4, 34, 6, 3, SSD1306_WHITE);
    int z = (millis() / 400) % 3;
    display.setCursor(108, 14 - z * 4);
    display.print(F("z"));
    if (z > 0) { display.setCursor(116, 8 - z * 3); display.print(F("Z")); }
    return;
  }
  if (millis() < blinkEnd) {
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
void drawHi() {
  drawEyes(26);
  fillBar(56, millis() - phaseStart, START_DELAY_MS);
}

void drawFocus() {
  display.setCursor(0, 0);
  if (task[0]) display.print(task);
  else { display.print(F("Focus #")); display.print(data.pomos + 1); }

  if (awayStart) {                       // stepped away, waiting up to 10 s
    unsigned long gone = millis() - awayStart;
    unsigned long left = gone >= AWAY_GRACE_MS ? 0 : (AWAY_GRACE_MS - gone + 999) / 1000;
    char buf[4];
    sprintf_P(buf, PSTR("%lu"), left);
    printCenteredF(F("Where'd you go?"), 14);
    display.setTextSize(3);
    printCentered(buf, 28);
    display.setTextSize(1);
    return;
  }
  if ((millis() / 500) % 2) display.fillCircle(124, 3, 3, SSD1306_WHITE);
  bigTime(focusLeft(), 18);
  fillBar(56, focusDone, (uint32_t)data.focusMin * 60UL);
}

void drawPaused() {
  printCenteredF(F("Paused"), 0);
  bigTime(focusLeft(), 16);
  if (presentSince) fillBar(56, millis() - presentSince, START_DELAY_MS);
  else printCenteredF(F("Sit down to resume"), 54);
}

void drawDone() {
  drawEyes(24);
  printCenteredF(F("Done! Break time"), 54);
}

void drawBreak() {
  uint32_t total = (uint32_t)data.breakMin * 60UL;
  uint32_t used = modeSec();
  printCenteredF(F("Break"), 0);
  bigTime(used >= total ? 0 : total - used, 16);
  if (someoneThere()) printCenteredF(F("Go take a break!"), 54);
  else                printCenteredF(F("Relax :)"), 54);
}

void drawStats() {
  char buf[12];
  display.setCursor(0, 0);  display.print(F("--- Lumi stats ---"));
  formatHM(data.todaySec, buf);
  display.setCursor(0, 14); display.print(F("Today:    ")); display.print(buf);
  display.setCursor(0, 26); display.print(F("Sessions: ")); display.print(data.pomos);
  display.setCursor(0, 38); display.print(F("Focus:    ")); display.print(data.focusMin); display.print(F(" min"));
  display.setCursor(0, 50); display.print(F("Break:    ")); display.print(data.breakMin); display.print(F(" min"));
}

void drawNote() {
  display.setTextSize(strlen(note) <= 10 ? 2 : 1);
  printCentered(note, 24);
  display.setTextSize(1);
}

void render() {
  unsigned long now = millis();
  if (now - lastFrame < 40) return;      // about 25 frames per second
  lastFrame = now;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (now < statsUntil)     drawStats();
  else if (now < noteUntil) drawNote();
  else {
    switch (phase) {
      case M_AWAY:
      case M_ASLEEP: drawEyes(32);  break;
      case M_HI:     drawHi();      break;
      case M_FOCUS:  drawFocus();   break;
      case M_PAUSED: drawPaused();  break;
      case M_DONE:   drawDone();    break;
      case M_BREAK:  drawBreak();   break;
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
  if (down && !btnWasDown) { btnDownAt = now; resetDone = false; }
  if (down && !resetDone && now - btnDownAt >= RESET_HOLD_MS) { newDay(); resetDone = true; }
  if (!down && btnWasDown && !resetDone && now - btnDownAt > 50) statsUntil = now + 4000;
  btnWasDown = down;
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
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true) {                       // OLED not found: blink the board LED
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(300);
    }
  }

  loadData();
  strcpy_P(note, PSTR("Hi! I'm Lumi"));
  noteUntil = millis() + 2500;
  nextBlink = millis() + 3000;
  moodUntil = millis() + 4000;
  phaseStart = millis();
  beep(1, 80);
}

void loop() {
  readCommands();
  updateButton();
  updateSensor();
  updateLogic();
  updateEyes();
  render();
  if (millis() - lastStatus >= 1000) sendStatus();
}
