// AIDev: Gemini 2.5 Pro Preview

#include <FastLED.h>
#include <ESP32Encoder.h>

// --- LED Panel Configuration ---
#define LED_DATA_PIN   4
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define NUM_LEDS       256 // 16x16
#define BRIGHTNESS     50
const int LED_COLUMNS = 16;
const int LED_ROWS    = 16;
CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // Global hue for pulsating colors

// --- Rotary Encoder Configuration ---
#define ENCODER_CLK_PIN  32
#define ENCODER_DT_PIN   33
#define ENCODER_SW_PIN   25
ESP32Encoder encoder;
long lastEncoderDetentPosition = 0;

// --- Timer State Machine ---
enum TimerState {
  IDLE,
  SET_TIME,
  RUNNING,
  FINISHED
};
TimerState currentState = IDLE;

// --- Timer Variables ---
int selectedMinutes = 1; // Default 1 min, range 1-90
unsigned long timerStartTime_ms = 0;
unsigned long timerDuration_ms = 0;

// --- Button Handling ---
int lastSwitchState = HIGH;
unsigned long lastButtonPressTime = 0;
const unsigned long DEBOUNCE_DELAY = 100; // ms
bool buttonActionPending = false;

// --- Font Data (5x7 pixels per digit) ---
const uint8_t font5x7_data[10][5] = {
  {0x3E, 0x51, 0x49, 0x51, 0x3E}, // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
  {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
  {0x22, 0x41, 0x49, 0x49, 0x36}, // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
  {0x2F, 0x49, 0x49, 0x49, 0x31}, // 5
  {0x3E, 0x4A, 0x49, 0x49, 0x30}, // 6
  {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
  {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
};

// --- Helper function to map X, Y to LED index for SERPENTINE layout ---
uint16_t XY(uint8_t x, uint8_t y) {
  if (x >= LED_COLUMNS || y >= LED_ROWS) {
    return NUM_LEDS; // Invalid coordinate, return an out-of-bounds index
  }

  uint16_t i;
  if (y % 2 == 0) {
    // Even rows (0, 2, 4,...): LEDs are indexed from left to right
    i = (y * LED_COLUMNS) + x;
  } else {
    // Odd rows (1, 3, 5,...): LEDs are indexed from right to left
    i = (y * LED_COLUMNS) + (LED_COLUMNS - 1 - x);
  }
  return i;
}

// --- Function to draw a character on the LED matrix ---
void drawChar(char c, int startX, int startY, CRGB color) {
  if (c < '0' || c > '9') return;
  uint8_t digitIndex = c - '0';

  for (int col = 0; col < 5; col++) { // 5 columns in font
    uint8_t colData = font5x7_data[digitIndex][col];
    for (int row = 0; row < 7; row++) { // 7 rows in font
      if ((colData >> row) & 0x01) { // Check if bit (pixel) is set
        uint16_t ledIndex = XY(startX + col, startY + row);
        if (ledIndex < NUM_LEDS) {
          leds[ledIndex] = color;
        }
      }
    }
  }
}

// --- Function to display a two-digit number ---
void displayNumberOnMatrix(int number, CRGB color) {
  FastLED.clearData(); // Clear LED buffer (don't show yet)

  if (number < 0 || number > 99) return; // Handles 00-99

  char S1 = (number / 10) + '0'; // Tens digit
  char S0 = (number % 10) + '0'; // Ones digit

  int digitWidth = 5;
  int digitHeight = 7;
  int spacing = 2; // Space between digits

  int totalWidth = digitWidth + spacing + digitWidth;
  // Center the number on the 16x16 display
  int startX = (LED_COLUMNS - totalWidth) / 2;
  int startY = (LED_ROWS - digitHeight) / 2;

  drawChar(S1, startX, startY, color);
  drawChar(S0, startX + digitWidth + spacing, startY, color);

  FastLED.show();
}


void setup() {
  Serial.begin(115200);
  delay(2000); // For power stability and serial connection

  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  encoder.attachFullQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN); // Or (ENCODER_CLK_PIN, ENCODER_DT_PIN) if direction is reversed
  encoder.clearCount(); // Start fresh

  currentState = IDLE;
  Serial.println("Visual Timer Ready. State: IDLE");
}

void handleButtonPress() {
  switch (currentState) {
    case IDLE:
      currentState = SET_TIME;
      selectedMinutes = 1; // Reset to default when entering set mode
      encoder.setCount(selectedMinutes * 4); // Sync encoder to current minutes (1 detent = 4 counts)
      lastEncoderDetentPosition = selectedMinutes; // Initialize based on selectedMinutes
      displayNumberOnMatrix(selectedMinutes, CRGB::CornflowerBlue);
      Serial.println("State: SET_TIME");
      break;

    case SET_TIME:
      currentState = RUNNING;
      timerStartTime_ms = millis();
      timerDuration_ms = (unsigned long)selectedMinutes * 60 * 1000;
      FastLED.clear();
      FastLED.show();
      Serial.print("State: RUNNING. Timer set for "); Serial.print(selectedMinutes); Serial.println(" minutes.");
      break;

    case RUNNING: // Button press during RUNNING cancels timer
    case FINISHED: // Button press after FINISHED resets
      currentState = IDLE;
      FastLED.clear();
      FastLED.show();
      Serial.println("State: IDLE (Timer Reset/Cancelled)");
      break;
  }
}

void loop() {
  // --- Button Check (Debounced) ---
  int currentSwitchState = digitalRead(ENCODER_SW_PIN);
  if (currentSwitchState == LOW && lastSwitchState == HIGH) {
    if (millis() - lastButtonPressTime > DEBOUNCE_DELAY) {
      buttonActionPending = true; // Signal that a debounced press occurred
      lastButtonPressTime = millis();
    }
  }
  lastSwitchState = currentSwitchState;

  if (buttonActionPending) {
    handleButtonPress();
    buttonActionPending = false; // Consume the action
  }

  // --- State Machine Logic ---
  switch (currentState) {
    case IDLE:
      // Screen is kept blank (done on transition to IDLE)
      break;

    case SET_TIME: {
      long rawEncoderPos = encoder.getCount();
      // Calculate current detent position from raw encoder counts
      // ESP32Encoder with attachFullQuad gives 4 counts per detent.
      long currentDetentValueFromEncoder = rawEncoderPos / 4;

      // We want to map this detent value to minutes.
      // If the encoder starts at 0, and selectedMinutes starts at 1,
      // we need a consistent way to map.
      // Let's use the change in detents to adjust selectedMinutes.

      if (currentDetentValueFromEncoder != lastEncoderDetentPosition) {
        int detentChange = currentDetentValueFromEncoder - lastEncoderDetentPosition;
        selectedMinutes += detentChange;
        selectedMinutes = constrain(selectedMinutes, 1, 90); // Range 01-90 min

        // Update lastEncoderDetentPosition to the new detent value
        lastEncoderDetentPosition = currentDetentValueFromEncoder;

        // If constraining changed selectedMinutes, we might want to resync the encoder's raw count
        // to avoid large jumps if the user hits the limit and keeps turning.
        // This ensures the encoder's "feel" matches the displayed value.
        if ( (selectedMinutes * 4) != rawEncoderPos ) {
             encoder.setCount(selectedMinutes * 4);
             // After setCount, the rawEncoderPos has changed, so update lastEncoderDetentPosition accordingly
             lastEncoderDetentPosition = selectedMinutes; // Since 1 min = 1 detent (conceptually)
        }


        displayNumberOnMatrix(selectedMinutes, CRGB::CornflowerBlue);
        Serial.print("Time selected: "); Serial.print(selectedMinutes);
        Serial.print(" (Encoder raw: "); Serial.print(rawEncoderPos);
        Serial.print(", Detent val: "); Serial.print(currentDetentValueFromEncoder);
        Serial.println(")");
      }
      break;
    }

    case RUNNING: {
      unsigned long currentTime_ms = millis();
      unsigned long elapsedTime_ms = currentTime_ms - timerStartTime_ms;

      if (elapsedTime_ms >= timerDuration_ms) {
        currentState = FINISHED;
        Serial.println("State: FINISHED (Timer Complete)");
        // Fill screen completely for FINISHED state (done once on transition)
        fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
        FastLED.show();
      } else {
        float progress = (float)elapsedTime_ms / timerDuration_ms;
        int ledsToLight = progress * NUM_LEDS;
        ledsToLight = constrain(ledsToLight, 0, NUM_LEDS);

        FastLED.clearData(); // Clear buffer
        for (int i = 0; i < ledsToLight; i++) {
          leds[i] = CHSV(gHue, 255, 255); // Pulsating color
        }
        FastLED.show();
        gHue++; // Cycle hue for pulsation
      }
      break;
    }

    case FINISHED:
      // Screen is filled with pulsating LEDs
      fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
      FastLED.show();
      gHue++; // Continue pulsating
      break;
  }
  delay(20); // General loop delay
}
