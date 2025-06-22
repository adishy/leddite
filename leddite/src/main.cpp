#include <Arduino.h>
#include <FastLED.h>

// Also include the other planned libraries to ensure they were installed correctly.
// We don't need to use them in this test, but compiling them validates the setup.
#include <ESP32Encoder.h>
#include <Adafruit_GFX.h>
#include <AceButton.h>

// --- LED Panel Configuration (match this to your hardware) ---
#define LED_DATA_PIN   4         // The GPIO pin for your LED data line
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define MATRIX_WIDTH   16
#define MATRIX_HEIGHT  16
#define NUM_LEDS       (MATRIX_WIDTH * MATRIX_HEIGHT) // 256
#define BRIGHTNESS     20        // Start with a safe, dim brightness

CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // Global hue variable for cycling colors

// --- Test State ---
int currentLed = 0; // The index of the LED to light up next
const int FILL_DELAY_MS = 25; // Delay in milliseconds between lighting each LED

void setup() {
  Serial.begin(115200);
  delay(2000); // Delay for power stability and for serial monitor to connect
  Serial.println("--- ESP32 Hardware Test Program ---");

  // --- Initialize FastLED ---
  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  // Clear the matrix on boot
  FastLED.clear();
  FastLED.show();
  
  Serial.println("FastLED Initialized.");
  Serial.println("Starting visual fill test...");
}

void loop() {
  // Check if we have filled the entire matrix
  if (currentLed >= NUM_LEDS) {
    // If full, wait for a moment
    Serial.println("Matrix full. Resetting test.");
    delay(1500);

    // Then clear the display and start over
    FastLED.clear();
    currentLed = 0;
  }

  // Light up the current LED with the current hue
  // The mapping from a linear index to a 2D grid is handled automatically
  // by how the LEDs are wired in the strip.
  leds[currentLed] = CHSV(gHue, 255, 255); // Hue, Saturation, Value

  // Show the updated LED
  FastLED.show();

  // Prepare for the next iteration
  currentLed++; // Move to the next LED
  gHue += 2;    // Shift the color slightly for the next LED

  // Wait a short moment before lighting the next one
  delay(FILL_DELAY_MS);
}