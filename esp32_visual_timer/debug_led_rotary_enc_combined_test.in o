// AIDev: Gemini 2.5 Pro Preview 
#include <FastLED.h>
#include <ESP32Encoder.h>

// --- LED Panel Configuration ---
#define LED_DATA_PIN   4     // GPIO pin for LED data
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define NUM_LEDS       256   // 16 rows * 16 columns
#define BRIGHTNESS     50    // Start with a moderate brightness (0-255)
CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // Global hue variable for cycling colors

// --- Rotary Encoder Configuration ---
#define ENCODER_CLK_PIN  32  // Pin A
#define ENCODER_DT_PIN   33  // Pin B
#define ENCODER_SW_PIN   25  // Switch pin
ESP32Encoder encoder;
long currentEncoderPosition = 0;

// --- Encoder Switch State ---
int lastSwitchState = HIGH; // Assuming INPUT_PULLUP

void setup() {
  Serial.begin(115200);
  delay(2000); // Delay for power stability and for serial monitor to connect

  Serial.println("ESP32 LED Panel & Rotary Encoder Test");

  // --- Initialize FastLED ---
  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  Serial.println("FastLED Initialized.");

  // --- Initialize Rotary Encoder ---
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);

  encoder.attachFullQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN); // Or (ENCODER_CLK_PIN, ENCODER_DT_PIN) if direction is reversed
  encoder.setCount(0); // Start encoder count at 0
  currentEncoderPosition = encoder.getCount();
  Serial.println("Rotary Encoder Initialized. Initial Count: " + String(currentEncoderPosition));
}

void loop() {
  // --- Read Rotary Encoder ---
  currentEncoderPosition = encoder.getCount();

  // Map encoder position to the number of LEDs to light up
  // Constrain the value to be between 0 and NUM_LEDS
  int numLedsToLight = constrain(currentEncoderPosition, 0, NUM_LEDS);

  // --- Update LEDs ---
  // First, clear all LEDs (or set to black)
  // This is simpler than figuring out which ones to turn off individually
  FastLED.clear(); // Clears to black

  // Light up LEDs from 0 to numLedsToLight-1 with the current hue
  for (int i = 0; i < numLedsToLight; i++) {
    leds[i] = CHSV(gHue, 255, 255); // Hue, Saturation, Value (Brightness controlled globally by FastLED.setBrightness)
  }
  FastLED.show();

  // Increment hue for the next frame, making colors cycle
  gHue++;

  // --- Check Encoder Switch ---
  int currentSwitchState = digitalRead(ENCODER_SW_PIN);
  if (currentSwitchState == LOW && lastSwitchState == HIGH) {
    Serial.println("Encoder Button Pressed!");
    // Optional: Reset encoder count on button press
    // encoder.setCount(0);
    // Serial.println("Encoder count reset to 0.");
    delay(50); // Simple debounce
  }
  lastSwitchState = currentSwitchState;

  // --- Serial Debug Output (optional, can be throttled) ---
  static unsigned long lastSerialPrintTime = 0;
  if (millis() - lastSerialPrintTime > 200) { // Print every 200ms
    Serial.print("Encoder: ");
    Serial.print(currentEncoderPosition);
    Serial.print(" | LEDs Lit: ");
    Serial.println(numLedsToLight);
    lastSerialPrintTime = millis();
  }

  delay(10); // Small delay for stability and responsiveness
}
