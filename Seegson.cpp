#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int potPin = A0;
const int btnPin = 6;
const int ledRed = 8;
const int ledGreen = 9;

int gameState = 0; 
uint16_t targetGlyph;
uint16_t options[5];
int correctIndex;
int selectedIndex = 0;
float timeLeft = 100.0;
bool btnState = false;
bool lastBtnState = false;

int hacksCompleted = 0;
const int totalHacksNeeded = 5;

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

void generateLevel() {
  targetGlyph = generateValidGlyph();
  correctIndex = random(0, 5);
  for (int i = 0; i < 5; i++) {
    if (i == correctIndex) {
      options[i] = targetGlyph;
    } else {
      uint16_t wrong;
      do {
        wrong = generateValidGlyph();
      } while (wrong == targetGlyph);
      options[i] = wrong;
    }
  }
}

void drawGlyph(int x, int y, uint16_t pattern, int scale) {
  for (int i = 0; i < 16; i++) {
    if (pattern & (1 << i)) {
      int col = i % 4;
      int row = i / 4;
      display.fillRect(x + col * 3 * scale, y + row * 3 * scale, 3 * scale - 1, 3 * scale - 1, SSD1306_WHITE);
    }
  }
}

void setup() {
  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(btnPin, INPUT); 
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println("SEVASTOPOL LINK v3.1");
  display.display();
  delay(400);

  display.println("TUNER CALIBRATION...");
  display.display();
  digitalWrite(ledRed, HIGH);
  delay(300);
  digitalWrite(ledRed, LOW);
  delay(100);
  digitalWrite(ledGreen, HIGH);
  delay(300);
  digitalWrite(ledGreen, LOW);

  display.println("CONNECTION ESTABLISHED");
  display.display();
  delay(800);

  timeLeft = 100.0; // Nastaveni casu pouze pri prvnim startu
  generateLevel();
}

void loop() {
  display.clearDisplay();
  
  int potVal = analogRead(potPin);
  selectedIndex = map(potVal, 0, 1024, 0, 5);
  if (selectedIndex > 4) selectedIndex = 4;

  btnState = digitalRead(btnPin);
  bool btnPressed = (btnState == HIGH && lastBtnState == LOW);
  lastBtnState = btnState;
  
  if (gameState == 0) {
    // Tlak stoupá s ubývajícím časem - červená ledka bliká, pokud je čas pod 30 %
    if (timeLeft < 30.0) {
      digitalWrite(ledRed, (millis() / 100) % 2);
    } else {
      digitalWrite(ledRed, HIGH);
    }
    digitalWrite(ledGreen, LOW);

    // Vykreslení postupu (ukazatele 5 splněných hacků) vlevo nahoře
    for (int i = 0; i < totalHacksNeeded; i++) {
      display.drawRect(2 + (i * 6), 2, 4, 4, SSD1306_WHITE);
      if (i < hacksCompleted) {
        display.fillRect(2 + (i * 6), 2, 4, 4, SSD1306_WHITE);
      }
    }

    // Zvětšený cílový znak pro lepší viditelnost (scale 3, centrovaný)
    drawGlyph(46, 2, targetGlyph, 3);

    display.drawRect(14, 42, 100, 4, SSD1306_WHITE);
    int barWidth = (int)timeLeft;
    if (barWidth > 0) {
      display.fillRect(14, 42, barWidth, 4, SSD1306_WHITE);
    }

    // Možnosti dole
    for (int i = 0; i < 5; i++) {
      int x = 12 + (i * 23);
      drawGlyph(x, 50, options[i], 1);
      
      if (i == selectedIndex) {
        display.drawRect(x - 2, 48, 15, 15, SSD1306_WHITE);
      }
    }

    if (btnPressed) {
      if (selectedIndex == correctIndex) {
        hacksCompleted++;
        if (hacksCompleted >= totalHacksNeeded) {
          gameState = 1; // Úspěšně hacknuto všech 5
        } else {
          generateLevel();
          timeLeft += 35.0; // Odměna: přidání času za rychlost
          if (timeLeft > 100.0) timeLeft = 100.0;
        }
      } else {
        gameState = 2; // Okamžitá chyba při překlepu!
      }
    }

    timeLeft -= 0.8; // Mnohem rychlejší ubývání času
    if (timeLeft <= 0) {
      gameState = 2;
    }
  } 
  else if (gameState == 1) {
    digitalWrite(ledRed, LOW);
    digitalWrite(ledGreen, HIGH);
    
    display.fillRect(0, 20, 128, 24, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(2);
    display.setCursor(28, 25);
    display.print("ACCEPTED");
    display.display();
    
    delay(2000);
    digitalWrite(ledGreen, LOW);
    
    hacksCompleted = 0; // Reset postupu
    timeLeft = 100.0;   // Reset času
    generateLevel();
    gameState = 0;
  }
  else if (gameState == 2) {
    digitalWrite(ledGreen, LOW);
    
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(35, 25);
    display.print("ERROR");
    display.display();

    // Trest zablikáním
    for(int i = 0; i < 6; i++) {
      digitalWrite(ledRed, HIGH);
      delay(150);
      digitalWrite(ledRed, LOW);
      delay(150);
    }
    
    delay(500);
    hacksCompleted = 0; // Trest: ztráta veškerého postupu
    timeLeft = 100.0;   // Reset času
    generateLevel();
    gameState = 0;
  }
  
  if (gameState == 0) {
    display.display();
    delay(20); // Zrychlený framerate
  }
}
