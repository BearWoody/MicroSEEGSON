#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int potPin = A0;
const int ledRed = 8;
const int ledGreen = 9;

int gameState = 0; 
float targetFreq;
float currentWave = 0;
int matchFrames = 0; 

void setup() {
  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println("W-Y CORP OS v1.2");
  display.display();
  delay(500);

  display.println("MEM: 64K OK");
  display.display();
  digitalWrite(ledRed, HIGH);
  delay(400);
  digitalWrite(ledRed, LOW);

  display.println("RF MOD: ONLINE");
  display.display();
  digitalWrite(ledGreen, HIGH);
  delay(500);
  digitalWrite(ledGreen, LOW);

  display.println("BYPASS: ACTIVE");
  display.display();
  delay(600);

  display.println("");
  display.println("AWAITING SYNC...");
  display.display();
  delay(1000);
  
  targetFreq = random(10, 50) / 100.0; 
}

void loop() {
  display.clearDisplay();
  int potVal = analogRead(potPin);
  
  if (gameState == 0) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("FREQUENCY SYNC");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    float playerFreq = map(potVal, 0, 1023, 10, 50) / 100.0;
    
    for (int x = 0; x < 128; x++) {
      if (x % 3 == 0) {
        int targetY = 32 + (sin(x * targetFreq + currentWave) * 16);
        display.drawPixel(x, targetY, SSD1306_WHITE);
      }
      int playerY = 32 + (sin(x * playerFreq + currentWave) * 16);
      display.drawPixel(x, playerY, SSD1306_WHITE);
    }
    
    currentWave += 0.15;
    
    float diff = abs(targetFreq - playerFreq);
    
    if (diff < 0.03) {
        digitalWrite(ledRed, HIGH);
        digitalWrite(ledGreen, LOW);
        
        matchFrames++;
        
        int barWidth = map(matchFrames, 0, 40, 0, 128);
        display.fillRect(0, 58, barWidth, 6, SSD1306_WHITE);
        
        if (matchFrames > 40) {
            gameState = 1; 
            matchFrames = 0;
        }
    } else {
        digitalWrite(ledRed, HIGH);
        digitalWrite(ledGreen, LOW);
        matchFrames = 0;
    }
  } 
  else if (gameState == 1) {
    digitalWrite(ledRed, LOW);
    
    display.setTextSize(2);
    display.setCursor(26, 18);
    display.print("ACCESS");
    display.setCursor(20, 38);
    display.print("GRANTED");
    display.display();
    
    digitalWrite(ledGreen, HIGH);
    delay(2000);
    digitalWrite(ledGreen, LOW);
    
    delay(500);
    
    gameState = 0;
    targetFreq = random(10, 50) / 100.0; 
  }
  
  if (gameState == 0) {
    display.display();
    delay(20); 
  }
}
