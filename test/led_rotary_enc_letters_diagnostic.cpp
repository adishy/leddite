// AIDev: Gemini 2.5 Pro Preview (Orientation & Input Diagnostic)

#include <Arduino.h>
#include <FastLED.h>

// --- LED Panel & Display Configuration ---
#define LED_DATA_PIN      4
#define LED_TYPE          WS2812B
#define COLOR_ORDER       GRB
#define NUM_LEDS          256 // 16x16
#define BRIGHTNESS        30
const int PANEL_WIDTH     = 16;
const int PANEL_HEIGHT    = 16;
CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // Global hue for animations

// --- Rotary Encoder Configuration ---
#define ENCODER_SW_PIN     25 // We only need the switch for this test

// --- Font Data (5x7) ---
const int FONT_CHAR_WIDTH  = 5;
const int FONT_CHAR_HEIGHT = 7;

// Font data for numbers 0-9
const uint8_t font5x7_numbers[10][5] = {
  {0x3E, 0x51, 0x49, 0x51, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46}, {0x22, 0x41, 0x49, 0x49, 0x36},
  {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x2F, 0x49, 0x49, 0x49, 0x31},
  {0x3E, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}
};

// Font data for letters 'H' and 'I'
const uint8_t font5x7_letters[2][5] = {
  {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
  {0x00, 0x41, 0x7F, 0x41, 0x00}  // I
};

// --- Test State ---
int counter = 1;
bool showHi = false;
unsigned long lastSwitchTime = 0;
const int SWITCH_DELAY_MS = 1500;

// --- Core Drawing & Mapping Functions (User Provided) ---

uint16_t XY_Serpentine_Original(uint8_t col, uint8_t row) {
  if (col >= PANEL_WIDTH || row >= PANEL_HEIGHT) {
    return NUM_LEDS;
  }
  uint16_t i;
  if (row % 2 == 0) { // Even rows: L to R
    i = (row * PANEL_WIDTH) + col;
  } else { // Odd rows: R to L
    i = (row * PANEL_WIDTH) + (PANEL_WIDTH - 1 - col);
  }
  return i;
}

void setPixel_Final(int final_canvas_x, int final_canvas_y, CRGB color) {
  if (final_canvas_x < 0 || final_canvas_x >= PANEL_WIDTH || 
      final_canvas_y < 0 || final_canvas_y >= PANEL_HEIGHT) {
    return;
  }
  int x_on_180_canvas = (PANEL_HEIGHT - 1) - final_canvas_y;
  int y_on_180_canvas = final_canvas_x;
  uint16_t serpentine_index_for_original_panel = XY_Serpentine_Original(x_on_180_canvas, y_on_180_canvas);
  uint16_t physical_led_index = (NUM_LEDS - 1) - serpentine_index_for_original_panel;
  if (physical_led_index < NUM_LEDS) {
    leds[physical_led_index] = color;
  }
}

void drawDigitOnCanvas(uint8_t digit, int startX, int startY, CRGB color) {
  if (digit > 9) return;
  for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
    uint8_t colData = font5x7_numbers[digit][col];
    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
      if ((colData >> row) & 0x01) {
        setPixel_Final(startX + col, startY + row, color);
      }
    }
  }
}

void drawLetterOnCanvas(char letter, int startX, int startY, CRGB color) {
  uint8_t letterIndex;
  if (letter == 'H') letterIndex = 0;
  else if (letter == 'I') letterIndex = 1;
  else return;

  for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
    uint8_t colData = font5x7_letters[letterIndex][col];
    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
      if ((colData >> row) & 0x01) {
        setPixel_Final(startX + col, startY + row, color);
      }
    }
  }
}

void displayNumber(int number) {
  FastLED.clear();
  int spacing = (number >= 10) ? 2 : 0;
  int totalWidth = (number >= 10) ? (FONT_CHAR_WIDTH * 2 + spacing) : FONT_CHAR_WIDTH;
  int startX = (PANEL_WIDTH - totalWidth) / 2;
  int startY = (PANEL_HEIGHT - FONT_CHAR_HEIGHT) / 2;

  if (number >= 10) {
    drawDigitOnCanvas(number / 10, startX, startY, CRGB::SkyBlue);
    drawDigitOnCanvas(number % 10, startX + FONT_CHAR_WIDTH + spacing, startY, CRGB::SkyBlue);
  } else {
    drawDigitOnCanvas(number, startX, startY, CRGB::SkyBlue);
  }
  FastLED.show();
}

void displayHI() {
  FastLED.clear();
  int spacing = 2;
  int totalWidth = FONT_CHAR_WIDTH * 2 + spacing;
  int startX = (PANEL_WIDTH - totalWidth) / 2;
  int startY = (PANEL_HEIGHT - FONT_CHAR_HEIGHT) / 2;
  
  drawLetterOnCanvas('H', startX, startY, CRGB::MediumPurple);
  drawLetterOnCanvas('I', startX + FONT_CHAR_WIDTH + spacing, startY, CRGB::MediumPurple);
  FastLED.show();
}

void runRainbowSplash() {
  fill_rainbow(leds, NUM_LEDS, gHue, 7);
  gHue++;
  FastLED.show();
}

// --- Main Setup and Loop ---

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- Orientation & Input Diagnostic ---");

  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);
  
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
}

void loop() {
  // Check the raw button state
  int buttonState = digitalRead(ENCODER_SW_PIN);

  if (buttonState == LOW) { // Button is being held down
    runRainbowSplash();
  } else { // Button is released
    if (millis() - lastSwitchTime > SWITCH_DELAY_MS) {
      lastSwitchTime = millis();
      showHi = !showHi; // Toggle between showing "HI" and the counter

      if (!showHi) {
        // If we're not showing HI, increment the counter and display it
        counter++;
        if (counter > 10) {
          counter = 1;
        }
        displayNumber(counter);
        Serial.print("Displaying number: "); Serial.println(counter);
      } else {
        // Otherwise, display "HI"
        displayHI();
        Serial.println("Displaying: HI");
      }
    }
  }
  
  delay(20); // Small delay for stability
}