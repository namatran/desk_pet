// Desk Pet Pomodoro — Arduino Nano
// Parts: LCD1602 (parallel), potentiometer, KY-040 rotary encoder,
//        RGB LED module, buzzer (passive sounds best)
//
// Controls:
//   Turn knob (when READY) -> set focus minutes (1-60)
//   Short press            -> start / pause / resume
//   Long press (1s)        -> reset to READY

#include <LiquidCrystal.h>

// ---------- Pins ----------
LiquidCrystal lcd(12, 11, 5, A0, A1, A2);   // RS, E, D4, D5, D6, D7
const byte ENC_CLK = 2, ENC_DT = 4, ENC_SW = A3;
const byte BUZZER = 3;
const byte LED_R = 9, LED_G = 10, LED_B = 6;

// ---------- Custom characters (5x8) ----------
byte eyeOpen[8]    = {0x00, 0x0E, 0x1F, 0x1B, 0x1F, 0x0E, 0x00, 0x00};
byte eyeClosed[8]  = {0x00, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
byte eyeHappy[8]   = {0x00, 0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00};
byte eyeSleepy[8]  = {0x00, 0x00, 0x00, 0x1F, 0x1F, 0x0E, 0x00, 0x00};
byte mouthSmile[8] = {0x00, 0x00, 0x00, 0x11, 0x0E, 0x00, 0x00, 0x00};
byte mouthFlat[8]  = {0x00, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
byte heart[8]      = {0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00};
enum Glyph { EYE_OPEN, EYE_CLOSED, EYE_HAPPY, EYE_SLEEPY, MOUTH_SMILE, MOUTH_FLAT, HEART };

// ---------- State ----------
enum Mode { READY, FOCUS, BREAK_TIME, PAUSED };
Mode mode = READY;
Mode pausedFrom = FOCUS;

int focusMin = 25;
const int BREAK_MIN = 5;
const unsigned long SLEEP_AFTER_MS = 60000UL;   // pet falls asleep if ignored

unsigned long endAt = 0, pausedLeft = 0;
unsigned long lastActivity = 0, lastDraw = 0;
unsigned long nextBlink = 0, blinkEnd = 0;
bool blinking = false;
byte sessions = 0;

// Button
bool btnDown = false, longFired = false;
unsigned long btnT = 0;

// Encoder
int lastClk = HIGH;

// Beeps (non-blocking)
byte beepsLeft = 0;
unsigned long nextBeep = 0;

// ---------- Helpers ----------
void setColor(byte r, byte g, byte b) {
  // Common-cathode LED. If yours is common-anode, use 255 - value.
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}

void startBeeps(byte n) { beepsLeft = n; nextBeep = millis(); }

void handleBeeps() {
  if (beepsLeft && (long)(millis() - nextBeep) >= 0) {
    tone(BUZZER, 1760, 120);
    beepsLeft--;
    nextBeep = millis() + 250;
  }
}

unsigned long timeLeft() {
  if (mode == PAUSED) return pausedLeft;
  if (mode == READY) return (unsigned long)focusMin * 60000UL;
  long d = (long)(endAt - millis());
  return d > 0 ? (unsigned long)d : 0;
}

void startTimer(Mode m, int minutes) {
  mode = m;
  endAt = millis() + (unsigned long)minutes * 60000UL;
}

// ---------- Input actions ----------
void onShortPress() {
  lastActivity = millis();
  switch (mode) {
    case READY:
      startTimer(FOCUS, focusMin);
      startBeeps(1);
      break;
    case FOCUS:
    case BREAK_TIME:
      pausedFrom = mode;
      pausedLeft = timeLeft();
      mode = PAUSED;
      break;
    case PAUSED:
      mode = pausedFrom;
      endAt = millis() + pausedLeft;
      break;
  }
}

void onLongPress() {
  lastActivity = millis();
  mode = READY;
  noTone(BUZZER);
  beepsLeft = 0;
}

void onTurn(int dir) {
  lastActivity = millis();
  if (mode == READY) focusMin = constrain(focusMin + dir, 1, 60);
}

void handleButton() {
  bool pressed = digitalRead(ENC_SW) == LOW;
  unsigned long now = millis();
  if (pressed && !btnDown) {
    btnDown = true; btnT = now; longFired = false;
  } else if (pressed && btnDown && !longFired && now - btnT > 1000) {
    longFired = true; onLongPress();
  } else if (!pressed && btnDown) {
    btnDown = false;
    if (!longFired && now - btnT > 30) onShortPress();   // 30ms debounce
  }
}

void handleEncoder() {
  int clk = digitalRead(ENC_CLK);
  if (clk != lastClk && clk == LOW) {
    // Swap 1 and -1 if the knob feels backwards
    onTurn(digitalRead(ENC_DT) != clk ? 1 : -1);
  }
  lastClk = clk;
}

// ---------- Timer + pet logic ----------
void handleTimer() {
  if (mode == FOCUS && timeLeft() == 0) {
    sessions++;
    startBeeps(3);
    startTimer(BREAK_TIME, BREAK_MIN);
  } else if (mode == BREAK_TIME && timeLeft() == 0) {
    startBeeps(2);
    mode = READY;
    lastActivity = millis();
  }
}

void handleBlink() {
  unsigned long now = millis();
  if (!blinking && (long)(now - nextBlink) >= 0) {
    blinking = true; blinkEnd = now + 150;
  } else if (blinking && (long)(now - blinkEnd) >= 0) {
    blinking = false; nextBlink = now + random(2500, 6000);
  }
}

void draw() {
  byte eye = EYE_OPEN, mouth = MOUTH_SMILE;
  const char* status = "ready?";
  bool asleep = (mode == READY) && (millis() - lastActivity > SLEEP_AFTER_MS);

  switch (mode) {
    case READY:
      if (asleep) { eye = EYE_SLEEPY; mouth = MOUTH_FLAT; status = "zzz..."; setColor(0, 0, 0); }
      else        { eye = EYE_OPEN;   mouth = MOUTH_SMILE; status = "ready?"; setColor(20, 20, 20); }
      break;
    case FOCUS:
      eye = EYE_OPEN; mouth = MOUTH_FLAT; status = "focus!"; setColor(0, 40, 0);
      break;
    case BREAK_TIME:
      eye = EYE_HAPPY; mouth = MOUTH_SMILE; status = "break time"; setColor(0, 0, 40);
      break;
    case PAUSED:
      eye = EYE_OPEN; mouth = MOUTH_FLAT; status = "paused"; setColor(40, 25, 0);
      break;
  }
  if (blinking && eye == EYE_OPEN) eye = EYE_CLOSED;

  // Row 0: face + status
  lcd.setCursor(0, 0);
  lcd.print('(');
  lcd.write(byte(eye));
  lcd.write(byte(mouth));
  lcd.write(byte(eye));
  lcd.print(") ");
  char buf[11];
  snprintf(buf, sizeof(buf), "%-10s", status);
  lcd.print(buf);

  // Row 1: timer + completed sessions
  unsigned long secs = (timeLeft() + 999) / 1000;
  char line[17];
  snprintf(line, sizeof(line), " %02u:%02u   ", (unsigned)(secs / 60), (unsigned)(secs % 60));
  lcd.setCursor(0, 1);
  lcd.print(line);
  lcd.setCursor(10, 1);
  lcd.write(byte(HEART));
  snprintf(line, sizeof(line), "x%-4u", sessions);
  lcd.print(line);
}

// ---------- Setup / loop ----------
void setup() {
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  lcd.begin(16, 2);
  lcd.createChar(EYE_OPEN, eyeOpen);
  lcd.createChar(EYE_CLOSED, eyeClosed);
  lcd.createChar(EYE_HAPPY, eyeHappy);
  lcd.createChar(EYE_SLEEPY, eyeSleepy);
  lcd.createChar(MOUTH_SMILE, mouthSmile);
  lcd.createChar(MOUTH_FLAT, mouthFlat);
  lcd.createChar(HEART, heart);
  lcd.clear();

  randomSeed(analogRead(A6));
  lastClk = digitalRead(ENC_CLK);
  lastActivity = millis();
  nextBlink = millis() + 3000;
}

void loop() {
  handleEncoder();
  handleButton();
  handleTimer();
  handleBeeps();
  handleBlink();

  if (millis() - lastDraw > 100) {
    lastDraw = millis();
    draw();
  }
}
