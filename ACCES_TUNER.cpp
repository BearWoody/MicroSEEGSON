/**
 * ============================================================================
 * SEVASTOPOL OS - ACCESS TUNER (ALIEN: ISOLATION REPLICA)
 * ============================================================================
 * * Hardware Configuration & Wiring:
 * --------------------------------
 * - OLED Display (128x64 I2C):
 * SDA -> Pin A4
 * SCL -> Pin A5
 * VCC -> 5V, GND -> GND
 * * - Analog Tuner (Potentiometer):
 * SIG (Wiper) -> Pin A0
 * VCC -> 5V, GND -> GND
 * * - Input Button (Active HIGH):
 * Signal -> Pin D6 (Requires a 10k pull-down resistor to GND)
 * VCC -> 5V
 * * - Status Indicators (LEDs):
 * Red LED (Error/Stress) -> Pin D8 (via 220-ohm resistor to GND)
 * Green LED (Success)    -> Pin D9 (via 220-ohm resistor to GND)
 * * - System Output (Relay/Lock mechanism):
 * Signal -> Pin D7 (Outputs 5V for 5 seconds upon successful bypass)
 * ============================================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Hardware pin definitions
const int potPin = A0;
const int btnPin = 6;
const int ledRed = 8;
const int ledGreen = 9;
const int pinUnlock = 7;

// Game State Machine: 
// 0 = Signal Tracking, 1 = Callback Code, 2 = Success/Unlock, 3 = Error/Reset
int gameState = 0; 

// --- PHASE 1 VARIABLES: SIGNAL TRACKING ---
float targetSignal;
int lockFrames = 0;

// --- PHASE 2 VARIABLES: CALLBACK CODE ---
const int sequenceLength = 4;
uint16_t targetSequence[sequenceLength];
int currentStep = 0;

uint16_t options[5];
int correctIndex;
int selectedIndex = 0;
float timeLeft = 100.0;

bool btnState = false;
bool lastBtnState = false;

/**
 * Generates a valid 16-bit pseudo-random glyph (4x4 matrix).
 * Ensures the glyph has between 5 and 11 active pixels to maintain visual clarity.
 */
uint16_t generateValidGlyph() {
  uint16_t pat = 0;
  int count = 0;
  while (count < 5 || count > 11) {
    pat = random(1, 65535);
    count = 0;
    for (int i = 0; i < 16; i++) {
      if (pat & (1 << i)) count++;
    }
  }
  return pat;
}

/**
 * Initializes the master sequence of glyphs that the user must decrypt.
 */
void generateSequence() {
  for (int i = 0; i < sequenceLength; i++) {
    targetSequence[i] = generateValidGlyph();
  }
  currentStep = 0;
}

/**
 * Generates 5 multiple-choice options for the current sequence step.
 * Injects the correct glyph into a random position and fills the rest with decoys.
 */
void generateOptions() {
  correctIndex = random(0, 5);
  for (int i = 0; i < 5; i++) {
    if (i == correctIndex) {
      options[i] = targetSequence[currentStep];
    } else {
      uint16_t wrong;
      do {
        wrong = generateValidGlyph();
      } while (wrong == targetSequence[currentStep]);
      options[i] = wrong;
    }
  }
}

/**
 * Renders a 16-bit integer as a 4x4 pixel art glyph on the OLED display.
 */
void drawGlyph(int x, int y, uint16_t pattern, int scale) {
  for (int i = 0; i < 16; i++) {
    if (pattern & (1 << i)) {
      int col = i % 4;
      int row = i / 4;
      display.fillRect(x + col * 3 * scale, y + row * 3 * scale, 3 * scale - 1, 3 * scale - 1, SSD1306_WHITE);
    }
  }
}

/**
 * Maps a percentage (0-100) to X/Y coordinates along the perimeter of the display.
 * Used for the hidden signal tracking cursor in Phase 1.
 */
void getPerimeterCoords(float percentage, int &x, int &y) {
    int rectX = 2, rectY = 14, rectW = 123, rectH = 47;
    float p = (percentage / 100.0) * (rectW * 2 + rectH * 2);
    
    if (p < rectW) { 
        x = rectX + p; 
        y = rectY; 
    } else if (p < rectW + rectH) { 
        x = rectX + rectW; 
        y = rectY + (p - rectW); 
    } else if (p < rectW * 2 + rectH) { 
        x = rectX + rectW - (p - (rectW + rectH)); 
        y = rectY + rectH; 
    } else { 
        x = rectX; 
        y = rectY + rectH - (p - (rectW * 2 + rectH)); 
    }
}

void setup() {
  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(btnPin, INPUT); 
  
  // Output relay initialization (secure default state)
  pinMode(pinUnlock, OUTPUT);
  digitalWrite(pinUnlock, LOW);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // Halt execution on display failure
  }
  
  // OS Boot Sequence
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println("SEVASTOPOL OS v7.1");
  display.display();
  delay(400);

  display.println("ACCESS TUNER: READY");
  display.display();
  digitalWrite(ledRed, HIGH);
  delay(300);
  digitalWrite(ledRed, LOW);
  digitalWrite(ledGreen, HIGH);
  delay(300);
  digitalWrite(ledGreen, LOW);
  delay(800);

  // Initialize randomized starting signal
  targetSignal = random(10, 90); 
}

void loop() {
  display.clearDisplay();
  
  // Apply low-pass filter to potentiometer input to mitigate analog noise/jitter
  static float smoothPot = 0;
  smoothPot = (smoothPot * 0.7) + (analogRead(potPin) * 0.3);
  int potVal = constrain((int)smoothPot, 40, 980);
  
  // Process button state (edge detection)
  btnState = digitalRead(btnPin);
  bool btnPressed = (btnState == HIGH && lastBtnState == LOW);
  lastBtnState = btnState;
  
  // ==========================================
  // STATE 0: PHASE 1 - SIGNAL TRACKING
  // ==========================================
  if (gameState == 0) {
    digitalWrite(ledGreen, LOW);
    
    // Calculate difference between player tuner position and hidden target
    float playerSignal = map(potVal, 40, 980, 0, 100);
    float diff = abs(targetSignal - playerSignal);
    
    // Handle circular wrap-around distance
    if (diff > 50.0) diff = 100.0 - diff;
    
    bool isLocking = (diff < 3.0);

    // Blink 'SIGNAL TRACKING' indicator when attempting signal lock
    if (!isLocking || (millis() / 200) % 2 == 0) {
      display.setTextSize(1);
      display.setCursor(46, 20);
      display.print("SIGNAL");
      display.setCursor(40, 32);
      display.print("TRACKING");
    }

    // Render perimeter tracker
    int px, py;
    getPerimeterCoords(playerSignal, px, py);
    display.fillRect(px - 2, py - 2, 5, 5, SSD1306_WHITE);

    // Render dynamic visual noise inversely proportional to signal proximity
    int noiseLevel = diff * 4; 
    for(int i = 0; i < noiseLevel; i++) {
       display.drawPixel(random(128), random(12, 64), SSD1306_WHITE);
    }

    // Evaluate signal lock criteria
    if (isLocking) {
        digitalWrite(ledRed, (millis() / 80) % 2); // Pulse stress LED
        lockFrames++;
        
        display.setCursor(42, 45);
        display.print("LOCKING");
        
        // Render lock progress bar
        int barWidth = map(lockFrames, 0, 30, 0, 60);
        display.drawRect(34, 55, 60, 4, SSD1306_WHITE);
        display.fillRect(34, 55, barWidth, 4, SSD1306_WHITE);
        
        if (lockFrames > 30) {
            // Signal acquired - Transition to Phase 2
            gameState = 1; 
            lockFrames = 0;
            timeLeft = 100.0;
            generateSequence();
            generateOptions();
        }
    } else {
        digitalWrite(ledRed, HIGH); 
        lockFrames = 0;
    }
  } 
  // ==========================================
  // STATE 1: PHASE 2 - CALLBACK CODE
  // ==========================================
  else if (gameState == 1) {
    // Escalate visual stress if time is critically low
    if (timeLeft < 30.0) digitalWrite(ledRed, (millis() / 100) % 2);
    else digitalWrite(ledRed, HIGH);

    // UI Header
    display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(26, 2);
    display.print("CALLBACK CODE");

    // Render remaining sequence symbols
    int startX = 16;
    for (int i = 0; i < sequenceLength; i++) {
      if (i >= currentStep) {
        drawGlyph(startX + (i * 26), 16, targetSequence[i], 2);
      }
    }

    // Render countdown timer bar
    display.drawRect(14, 45, 100, 4, SSD1306_WHITE);
    int barWidth = (int)timeLeft;
    if (barWidth > 0) {
      display.fillRect(14, 45, barWidth, 4, SSD1306_WHITE);
    }

    // Process player selection
    selectedIndex = map(potVal, 40, 981, 0, 5);
    if (selectedIndex > 4) selectedIndex = 4;

    // Render multiple choice options
    for (int i = 0; i < 5; i++) {
      int x = 12 + (i * 23);
      drawGlyph(x, 52, options[i], 1);
      if (i == selectedIndex) {
        display.drawRect(x - 2, 50, 15, 15, SSD1306_WHITE);
      }
    }

    // Evaluate input logic
    if (btnPressed) {
      if (selectedIndex == correctIndex) {
        currentStep++; // Correct symbol selected
        
        if (currentStep >= sequenceLength) {
          gameState = 2; // Decryption complete
        } else {
          generateOptions(); // Procedurally generate next batch of options
          timeLeft += 20.0; // Time reward for accuracy
          if (timeLeft > 100.0) timeLeft = 100.0;
        }
      } else {
        gameState = 3; // Incorrect symbol triggers instant failure
      }
    }

    // Decrement timer
    timeLeft -= 0.6; 
    if (timeLeft <= 0) {
      gameState = 3; // Timeout triggers instant failure
    }
  } 
  // ==========================================
  // STATE 2: SUCCESS & UNLOCK PROTOCOL
  // ==========================================
  else if (gameState == 2) {
    digitalWrite(ledRed, LOW);
    digitalWrite(ledGreen, HIGH);
    
    // Trigger external hardware relay (Unlock mechanism)
    digitalWrite(pinUnlock, HIGH); 
    
    display.fillRect(0, 20, 128, 24, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(2);
    display.setCursor(15, 25);
    display.print("UNLOCKED");
    display.display();
    
    delay(5000); // Maintain unlocked state for 5 seconds
    
    // Re-secure the system and reset the tuner
    digitalWrite(ledGreen, LOW);
    digitalWrite(pinUnlock, LOW); 
    
    targetSignal = random(10, 90); 
    gameState = 0; 
  }
  // ==========================================
  // STATE 3: CRITICAL ERROR & PUNISHMENT
  // ==========================================
  else if (gameState == 3) {
    digitalWrite(ledGreen, LOW);
    
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(35, 25);
    display.print("ERROR");
    display.display();

    // Visual punishment via strobe effect
    for(int i = 0; i < 6; i++) {
      digitalWrite(ledRed, HIGH);
      delay(150);
      digitalWrite(ledRed, LOW);
      delay(150);
    }
    
    delay(500);
    
    // Penalize player by randomizing the signal target and returning to Phase 1
    targetSignal = random(10, 90); 
    gameState = 0; 
  }
  
  // Render frame (Only update display during interactive phases)
  if (gameState == 0 || gameState == 1) {
    display.display();
    delay(20); 
  }
}
