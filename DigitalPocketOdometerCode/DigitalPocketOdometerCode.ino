#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoLowPower.h>

// Screen Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C // Standard I2C address for 128x64 OLEDs
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Hardware Pin Definitions
const int pinCLK = 8;
const int pinDT = 9;
const int pinSW = 10;

// Odometer & Mechanical Constants
const float WHEEL_DIAMETER_MM = 19.50;
const float WHEEL_DIAMETER_CM = WHEEL_DIAMETER_MM / 10.0;
// Standard EC11 encoders typically have 20 pulses per revolution (detents). 
// If your distance is off by a factor of 2 or 4, adjust this to 10.0, 30.0, or 40.0.
const float PULSES_PER_REV = 20.0; 
const float WHEEL_CIRCUMFERENCE_CM = WHEEL_DIAMETER_CM * PI;
const float CM_PER_PULSE = WHEEL_CIRCUMFERENCE_CM / PULSES_PER_REV;

// Global variables modified inside ISRs must be volatile
volatile long rotationValue = 0;
volatile bool updateDisplayFlag = true;
volatile unsigned long lastInteractionTime = 0;

// Button State Machine Variables
volatile bool pendingSingleClick = false;
volatile unsigned long lastClickTime = 0;
const unsigned long debounceDelay = 50;        // 50ms filter for mechanical button bounce
const unsigned long doubleClickWindow = 350;   // 350ms max window between clicks for a double-click

// UI State
bool showInches = false; // false = Centimeters, true = Inches

// Configuration Constants
const unsigned long sleepTimeout = 15000; // 15 seconds in milliseconds

// Interrupt Service Routine: Triggered when the wheel rolls (CLK transitions HIGH to LOW)
void handleEncoder() {
  // Read DT pin to evaluate direction on the falling edge of CLK
  if (digitalRead(pinDT) == HIGH) {
    rotationValue++;
  } else {
    rotationValue--;
  }
  lastInteractionTime = millis();
  updateDisplayFlag = true;
}

// Interrupt Service Routine: Triggered when the roller/button is clicked down
void handleButton() {
  unsigned long currentTime = millis();
  
  // Software debounce filter to ignore mechanical button noise
  if (currentTime - lastClickTime < debounceDelay) {
    return; 
  }
  
  // Evaluate if this click falls within the double-click time window
  if (pendingSingleClick && (currentTime - lastClickTime <= doubleClickWindow)) {
    // --- DOUBLE CLICK DETECTED ---
    rotationValue = 0;           // Reset odometer count to zero
    pendingSingleClick = false;  // Cancel the pending single click action
    updateDisplayFlag = true;
    lastClickTime = 0;           // Reset timestamp so a 3rd click isn't seen as a double-click
  } else {
    // --- FIRST CLICK OF POTENTIAL DOUBLE CLICK ---
    pendingSingleClick = true;
    lastClickTime = currentTime;
  }
  
  lastInteractionTime = currentTime;
}

void setup() {
  // Initialize pins with internal pull-up resistors active
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);

  // Initialize the OLED Screen
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;); // Halt execution permanently if the screen cannot be detected
  }
  
  // Set initial text formatting parameters
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  // LowPower wake-up attachments automatically assign hardware interrupts 
  // and keep the SAMD21 External Interrupt Controller active during sleep.
  LowPower.attachInterruptWakeup(pinCLK, handleEncoder, FALLING);
  LowPower.attachInterruptWakeup(pinSW, handleButton, FALLING);

  // Set up the first active loop window runtime
  lastInteractionTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- NON-BLOCKING BUTTON TIMEOUT CHECK ---
  // If a single click is pending and the double-click window has expired without a second click:
  if (pendingSingleClick && (currentMillis - lastClickTime > doubleClickWindow)) {
    pendingSingleClick = false;
    showInches = !showInches; // Toggle between Centimeters and Inches
    updateDisplayFlag = true;
    lastInteractionTime = currentMillis;
  }

  // --- DISPLAY RENDERING ---
  // Render updates to the UI screen cleanly without blocking CPU timing
  if (updateDisplayFlag) {
    updateDisplayFlag = false;
    
    // Calculate actual physical distance
    float distanceCM = rotationValue * CM_PER_PULSE;
    float displayDistance = showInches ? (distanceCM / 2.54) : distanceCM;
    
    display.clearDisplay();
    
    // Render Dynamic Header Label based on current units
    display.setTextSize(1);
    display.setCursor(0, 4);
    if (showInches) {
      display.print("DISTANCE (INCHES)");
    } else {
      display.print("DISTANCE (CM)");
    }
    
    // Draw a divider graphic line
    display.drawFastHLine(0, 16, SCREEN_WIDTH, SSD1306_WHITE);
    
    // Render the calculated distance value with 2 decimal places
    display.setTextSize(3);
    display.setCursor(0, 28);
    display.print(displayDistance, 2);
    
    display.display();
  }

  // --- POWER MANAGEMENT & SLEEP ---
  // Monitor inactive elapsed time boundaries (only sleep if no button clicks are pending resolution)
  if (!pendingSingleClick && (millis() - lastInteractionTime > sleepTimeout)) {
    
    // Explicitly send the power-down command to the OLED driver chip
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    
    // Freeze CPU system clocks and enter deep sleep mode.
    // The program execution will completely pause exactly on this line 
    // until a FALLING edge logic state occurs on pinCLK or pinSW.
    LowPower.deepSleep();
    
    // --- THE GADGET WAKES UP HERE ---
    
    // Re-awaken and power back up the OLED display pixels
    display.ssd1306_command(SSD1306_DISPLAYON);
    
    // Update timestamps immediately to keep the device awake for another 15 seconds
    lastInteractionTime = millis();
    updateDisplayFlag = true;
  }
}