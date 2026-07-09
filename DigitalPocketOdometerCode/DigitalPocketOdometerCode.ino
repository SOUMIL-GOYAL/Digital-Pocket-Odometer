#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoLowPower.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int pinCLK = 8;
const int pinDT = 9;
const int pinSW = 10;

const float WHEEL_DIAMETER_MM = 19.50;
const float WHEEL_DIAMETER_CM = WHEEL_DIAMETER_MM / 10.0;
const float RUBBER_BAND_FACTOR = -1.15;

const float PULSES_PER_REV = 20.0; 
const float WHEEL_CIRCUMFERENCE_CM = WHEEL_DIAMETER_CM * PI;
const float CM_PER_PULSE = WHEEL_CIRCUMFERENCE_CM / PULSES_PER_REV;

volatile long rotationValue = 0;
volatile bool updateDisplayFlag = true;
volatile unsigned long lastInteractionTime = 0;

volatile bool pendingSingleClick = false;
volatile unsigned long lastClickTime = 0;
const unsigned long debounceDelay = 50;      
const unsigned long doubleClickWindow = 500;   

bool showInches = false; // false = Centimeters, true = Inches

const unsigned long sleepTimeout = 15000;

void handleEncoder() {
  if (digitalRead(pinDT) == HIGH) {
    rotationValue++;
  } else {
    rotationValue--;
  }
  lastInteractionTime = millis();
  updateDisplayFlag = true;
}

void handleButton() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastClickTime < debounceDelay) {
    return; 
  }
  
  if (pendingSingleClick && (currentTime - lastClickTime <= doubleClickWindow)) {
    rotationValue = 0;          
    pendingSingleClick = false; 
    updateDisplayFlag = true;
    lastClickTime = 0;
  } else {
    pendingSingleClick = true;
    lastClickTime = currentTime;
  }
  
  lastInteractionTime = currentTime;
}

void setup() {
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;);
  }
  
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  LowPower.attachInterruptWakeup(pinCLK, handleEncoder, FALLING);
  LowPower.attachInterruptWakeup(pinSW, handleButton, FALLING);

  lastInteractionTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  if (pendingSingleClick && (currentMillis - lastClickTime > doubleClickWindow)) {
    pendingSingleClick = false;
    showInches = !showInches; // Toggle between Centimeters and Inches
    updateDisplayFlag = true;
    lastInteractionTime = currentMillis;
  }

  if (updateDisplayFlag) {
    updateDisplayFlag = false;
    
    float distanceCM = rotationValue * CM_PER_PULSE;
    float displayDistance = showInches ? (distanceCM / 2.54) : distanceCM;
    
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setCursor(0, 4);
    display.print("POCKET ODOMETER");
    display.drawFastHLine(0, 16, SCREEN_WIDTH, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 28);

  if (showInches) {
    display.print(displayDistance * RUBBER_BAND_FACTOR, 2); 
    display.print(" in"); // Appends text right after the number
  } else {
    display.print(displayDistance* RUBBER_BAND_FACTOR, 2);
    display.print(" cm");
  }
    
    
    display.display();
  }

  if (!pendingSingleClick && (millis() - lastInteractionTime > sleepTimeout)) {
    
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    
    // The program execution will completely pause exactly on this line 
    LowPower.deepSleep();
    
    //WAKES UP HERE
    display.ssd1306_command(SSD1306_DISPLAYON);
    
    lastInteractionTime = millis();
    updateDisplayFlag = true;
  }
}