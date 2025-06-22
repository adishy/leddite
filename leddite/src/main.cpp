#include <Arduino.h>
#include <FastLED.h>
#include <ESP32Encoder.h>
#include <AceButton.h>

using namespace ace_button;

// --- Hardware Pin Definitions ---
#define LED_DATA_PIN   4     // GPIO pin for LED data
#define ENCODER_CLK_PIN  32  // Pin A
#define ENCODER_DT_PIN   33  // Pin B
#define ENCODER_SW_PIN   25  // Switch pin

// --- LED Panel Configuration ---
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define MATRIX_WIDTH   16
#define MATRIX_HEIGHT  16
#define NUM_LEDS       (MATRIX_WIDTH * MATRIX_HEIGHT) // 256
#define BRIGHTNESS     40

CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // Global hue for animations

// --- Rotary Encoder & Button Objects ---
ESP32Encoder encoder;
AceButton button(ENCODER_SW_PIN);

// Function prototype for the button event handler
void handleButtonEvent(AceButton* button, uint8_t eventType, uint8_t buttonState);

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("--- LED & Encoder Proportional Fill Test ---");
  Serial.println("Turn encoder to fill. Click to reset.");

  // --- Initialize FastLED ---
  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true); // Clear display and show()
  Serial.println("FastLED Initialized.");

  // --- Initialize Rotary Encoder ---
  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachFullQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN);
  encoder.clearCount();
  Serial.println("Rotary Encoder Initialized.");
  
  // --- Initialize AceButton ---
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  ButtonConfig* buttonConfig = button.getButtonConfig();
  buttonConfig->setEventHandler(handleButtonEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick); // We only care about single clicks for this simple test
  Serial.println("AceButton Initialized. Ready for input.");
}

// Event handler for button presses
void handleButtonEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  if (eventType == AceButton::kEventClicked) {
    Serial.println(">>> Button Click Detected! Resetting encoder count. <<<");
    encoder.clearCount();
  }
}

void loop() {
  // 1. Check for button events (non-blocking)
  button.check();

  // 2. Get current encoder position
  long encoderPosition = encoder.getCount();

  // 3. Map encoder position to the number of LEDs to light up
  // The constrain function ensures the value is between 0 and NUM_LEDS (256)
  int ledsToLight = constrain(encoderPosition, 0, NUM_LEDS);

  // 4. Update the LED display
  FastLED.clearData(); // Clear the internal buffer
  for (int i = 0; i < ledsToLight; i++) {
    // Fill with a color that cycles its hue based on the position
    leds[i] = CHSV(gHue + (i * 2), 255, 255);
  }
  FastLED.show(); // Push the data to the LEDs

  // 5. Provide continuous debug output, throttled to every 250ms
  static unsigned long lastDebugPrintTime = 0;
  if (millis() - lastDebugPrintTime > 250) {
    lastDebugPrintTime = millis();

    // Read the raw physical state of the button pin for debugging
    int rawButtonState = digitalRead(ENCODER_SW_PIN);

    Serial.print("Encoder Value: ");
    Serial.print(encoderPosition);
    Serial.print("  |  LEDs Lit: ");
    Serial.print(ledsToLight);
    Serial.print("  |  Button Pin State: ");
    Serial.println(rawButtonState == LOW ? "PRESSED" : "RELEASED");
  }
  
  delay(10); // Small delay for system stability
}