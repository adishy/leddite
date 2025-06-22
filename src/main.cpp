// AIDev: Gemini 2.5 Pro Preview (Final Visual Timer with AceButton & Debug)

#include <Arduino.h>
#include <FastLED.h>
#include <ESP32Encoder.h>
#include <AceButton.h>

using namespace ace_button;

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

// --- Font Data (5x7) ---
const int FONT_CHAR_WIDTH  = 5;
const int FONT_CHAR_HEIGHT = 7;
// Numbers 0-9
const uint8_t font5x7_numbers[10][5] = {
  {0x3E, 0x51, 0x49, 0x51, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46}, {0x22, 0x41, 0x49, 0x49, 0x36},
  {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x2F, 0x49, 0x49, 0x49, 0x31},
  {0x3E, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}
};
// Letters for "DONE"
const uint8_t font5x7_letters[4][5] = {
  {0x7F, 0x41, 0x41, 0x41, 0x3E}, // D
  {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
  {0x7F, 0x02, 0x04, 0x08, 0x7F}, // N
  {0x7F, 0x49, 0x49, 0x49, 0x41}  // E
};


// --- Input Device Configuration ---
#define ENCODER_CLK_PIN    32
#define ENCODER_DT_PIN     33
#define ENCODER_SW_PIN     25
#define ENCODER_STEPS_PER_DETENT 4
ESP32Encoder encoder;
AceButton button(ENCODER_SW_PIN);
long lastEncoderDetentPosition = 0;

// --- Timer State Machine & Variables ---
enum class TimerState { IDLE, SET_TIME, RUNNING, FINISHED };
TimerState currentState = TimerState::IDLE;
int selectedMinutes = 1;
unsigned long timerStartTime_ms = 0;
unsigned long timerDuration_ms = 0;
unsigned long lastDebugPrintTime = 0;
bool hasPlayedFinishAnimation = true; // Start as true to prevent animation on boot


// --- Core Drawing & Mapping Functions (User Confirmed) ---

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

void drawPixelOnCanvas(int x, int y, CRGB color) {
  setPixel_Final(x, y, color);
}

void drawNumberCharOnCanvas(uint8_t digit, int startX, int startY, CRGB color) {
  if (digit > 9) return;
  for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
    uint8_t colData = font5x7_numbers[digit][col];
    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
      if ((colData >> row) & 0x01) {
        drawPixelOnCanvas(startX + col, startY + row, color);
      }
    }
  }
}

void drawLetterOnCanvas(char letter, int startX, int startY, CRGB color) {
  uint8_t letterIndex;
  if (letter == 'D') letterIndex = 0;
  else if (letter == 'O') letterIndex = 1;
  else if (letter == 'N') letterIndex = 2;
  else if (letter == 'E') letterIndex = 3;
  else return;

  for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
    uint8_t colData = font5x7_letters[letterIndex][col];
    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
      if ((colData >> row) & 0x01) {
        drawPixelOnCanvas(startX + col, startY + row, color);
      }
    }
  }
}

void displayNumber(int number, CRGB color) {
  if (number < 0 || number > 99) return;
  int spacing = (number >= 10) ? 2 : 0;
  int totalWidth = (number >= 10) ? (FONT_CHAR_WIDTH * 2 + spacing) : FONT_CHAR_WIDTH;
  int startX = (PANEL_WIDTH - totalWidth) / 2;
  int startY = (PANEL_HEIGHT - FONT_CHAR_HEIGHT) / 2;

  if (number >= 10) {
    drawNumberCharOnCanvas(number / 10, startX, startY, color);
    drawNumberCharOnCanvas(number % 10, startX + FONT_CHAR_WIDTH + spacing, startY, color);
  } else {
    drawNumberCharOnCanvas(number, startX, startY, color);
  }
}

// --- NEW: Finished Animation ---
void playFinishedAnimation() {
    CRGB doneColor = CRGB::White;
    int letterSpacing = 2;

    // Calculate positions for "DO" on the top row
    int topRowTotalWidth = FONT_CHAR_WIDTH * 2 + letterSpacing;
    int topRowStartX = (PANEL_WIDTH - topRowTotalWidth) / 2;
    int topRowStartY = 1; // Near the top

    // Calculate positions for "NE" on the bottom row
    int bottomRowTotalWidth = FONT_CHAR_WIDTH * 2 + letterSpacing;
    int bottomRowStartX = (PANEL_WIDTH - bottomRowTotalWidth) / 2;
    int bottomRowStartY = topRowStartY + FONT_CHAR_HEIGHT + 1; // FIX: Pushed this row up by one pixel

    for (int i = 0; i < 3; i++) {
        // Show rainbow splash
        fill_rainbow(leds, NUM_LEDS, gHue, 7);
        FastLED.show();
        delay(500);

        // Show "DONE"
        FastLED.clear();
        drawLetterOnCanvas('D', topRowStartX, topRowStartY, doneColor);
        drawLetterOnCanvas('O', topRowStartX + FONT_CHAR_WIDTH + letterSpacing, topRowStartY, doneColor);
        drawLetterOnCanvas('N', bottomRowStartX, bottomRowStartY, doneColor);
        drawLetterOnCanvas('E', bottomRowStartX + FONT_CHAR_WIDTH + letterSpacing, bottomRowStartY, doneColor);
        FastLED.show();
        delay(500);
    }
}

// --- State Update Functions ---

void updateSetTimeState() {
  long rawEncoderPos = encoder.getCount();
  long currentDetent = rawEncoderPos / ENCODER_STEPS_PER_DETENT;
  if (currentDetent != lastEncoderDetentPosition) {
    int change = currentDetent - lastEncoderDetentPosition;
    selectedMinutes += change;
    selectedMinutes = constrain(selectedMinutes, 1, 99);
    encoder.setCount(selectedMinutes * ENCODER_STEPS_PER_DETENT);
    lastEncoderDetentPosition = selectedMinutes;
    Serial.print("[DEBUG] SET_TIME - New time set: "); Serial.println(selectedMinutes);
  }
  displayNumber(selectedMinutes, CRGB::CornflowerBlue);
}

void updateRunningState() {
  unsigned long elapsedTime_ms = millis() - timerStartTime_ms;
  if (elapsedTime_ms >= timerDuration_ms) {
    Serial.println("[STATE] Timer expired -> FINISHED");
    currentState = TimerState::FINISHED;
    return;
  }
  
  float progress = (float)elapsedTime_ms / timerDuration_ms;
  int ledsToLight = progress * NUM_LEDS;
  
  int pixelsDrawn = 0;
  for (int y = 0; y < PANEL_HEIGHT; y++) {
    for (int x = 0; x < PANEL_WIDTH; x++) {
      if (pixelsDrawn < ledsToLight) {
        drawPixelOnCanvas(x, y, CHSV(gHue, 255, 255));
        pixelsDrawn++;
      } else {
        goto end_progress_draw;
      }
    }
  }
  end_progress_draw:;
  
  gHue++;

  if (millis() - lastDebugPrintTime > 1000) {
    Serial.print("[DEBUG] RUNNING - Progress: ");
    Serial.print(progress * 100, 1);
    Serial.println("%");
    lastDebugPrintTime = millis();
  }
}

void updateFinishedState() {
  if (!hasPlayedFinishAnimation) {
    playFinishedAnimation();
    hasPlayedFinishAnimation = true;
  }
  // End on the rainbow splash screen, keep it dynamic
  fill_rainbow(leds, NUM_LEDS, gHue, 7);
  gHue++;
}

// --- Input Handling ---
void handleButtonEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  if (eventType != AceButton::kEventClicked) return;

  Serial.print("[INPUT] Click detected. ");
  
  switch (currentState) {
    case TimerState::IDLE:
      Serial.println("State: IDLE -> SET_TIME");
      currentState = TimerState::SET_TIME;
      selectedMinutes = 1;
      encoder.setCount(selectedMinutes * ENCODER_STEPS_PER_DETENT);
      lastEncoderDetentPosition = selectedMinutes;
      break;
    case TimerState::SET_TIME:
      Serial.println("State: SET_TIME -> RUNNING");
      currentState = TimerState::RUNNING;
      timerStartTime_ms = millis();
      timerDuration_ms = (unsigned long)selectedMinutes * 60 * 1000;
      hasPlayedFinishAnimation = false; // Arm the animation for when the timer finishes
      break;
    case TimerState::RUNNING:
    case TimerState::FINISHED:
      Serial.println("State: -> IDLE");
      currentState = TimerState::IDLE;
      break;
  }
}

// --- Main Setup and Loop ---

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- Visual Timer (AceButton & Debug) ---");

  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);

  // Configure Encoder
  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachFullQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN);
  encoder.clearCount();
  
  // Configure Button Pin with AceButton
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  ButtonConfig* buttonConfig = button.getButtonConfig();
  buttonConfig->setEventHandler(handleButtonEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  Serial.println("[SETUP] AceButton initialized for single clicks.");

  currentState = TimerState::IDLE;
  Serial.println("[STATE] Initial state: IDLE");
}

void loop() {
  button.check();

  FastLED.clearData();
  switch (currentState) {
    case TimerState::IDLE:     /* Nothing to do */    break;
    case TimerState::SET_TIME: updateSetTimeState();  break;
    case TimerState::RUNNING:  updateRunningState();  break;
    case TimerState::FINISHED: updateFinishedState(); break;
  }
  FastLED.show();
  
  delay(10);
}