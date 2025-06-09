// AIDev: Gemini 2.5 Pro Previiew

#include <FastLED.h>
#include <ESP32Encoder.h>

// --- LED Panel Configuration ---
#define LED_DATA_PIN   4
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define NUM_LEDS       256 // 16x16
#define BRIGHTNESS     32
const int PANEL_WIDTH  = 16; // Visual width of the final display
const int PANEL_HEIGHT = 16; // Visual height of the final display
CRGB leds[NUM_LEDS];
uint8_t gHue = 0;

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
int selectedMinutes = 1;
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
const int FONT_CHAR_WIDTH = 5;
const int FONT_CHAR_HEIGHT = 7;

// --- Base Serpentine XY Mapping for an UNROTATED Panel ---
// (0,0) is top-left of the unrotated panel.
// 'col' is the column index, 'row' is the row index.
uint16_t XY_Serpentine_Original(uint8_t col, uint8_t row) {
  if (col >= PANEL_WIDTH || row >= PANEL_HEIGHT) { // Using PANEL_WIDTH/HEIGHT as physical dimensions here
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

// --- Function to set a pixel on the display with final rotation ---
// (final_canvas_x, final_canvas_y) is the desired VISUAL position on the *final* output.
// (0,0) is top-left of this final view.
void setPixel_Final(int final_canvas_x, int final_canvas_y, CRGB color) {
  if (final_canvas_x < 0 || final_canvas_x >= PANEL_WIDTH || 
      final_canvas_y < 0 || final_canvas_y >= PANEL_HEIGHT) {
    return; // Out of bounds for the final visual canvas
  }

  // Step 1: Transform final_canvas coordinates to what they would be on the
  // intermediate 180-degree rotated canvas.
  // This is a +90 degree clockwise rotation of the canvas.
  // NewX = MaxY - OldY; NewY = OldX
  // Here, MaxY is (PANEL_HEIGHT - 1) because PANEL_HEIGHT is the height of the canvas being rotated.
  int x_on_180_canvas = (PANEL_HEIGHT - 1) - final_canvas_y;
  int y_on_180_canvas = final_canvas_x;


  // Step 2: Now, apply the logic from the previous setPixel_Rotated180,
  // using x_on_180_canvas and y_on_180_canvas as its input.
  // This input (x_on_180_canvas, y_on_180_canvas) was treated as the (col, row)
  // for the XY_Serpentine_Original function in that previous logic.
  uint16_t serpentine_index_for_original_panel = XY_Serpentine_Original(x_on_180_canvas, y_on_180_canvas);

  // Step 3: Apply the 180-degree flip to that serpentine index.
  uint16_t physical_led_index = (NUM_LEDS - 1) - serpentine_index_for_original_panel;

  if (physical_led_index < NUM_LEDS) {
    leds[physical_led_index] = color;
  }
}

// --- Function to draw a character using final canvas coordinates ---
void drawCharOnCanvas(char c, int char_start_final_x, int char_start_final_y, CRGB color) {
  if (c < '0' || c > '9') return;
  uint8_t digitIndex = c - '0';

  for (int font_col_offset = 0; font_col_offset < FONT_CHAR_WIDTH; font_col_offset++) {
    uint8_t columnPixelData = font5x7_data[digitIndex][font_col_offset];
    for (int font_row_offset = 0; font_row_offset < FONT_CHAR_HEIGHT; font_row_offset++) {
      if ((columnPixelData >> font_row_offset) & 0x01) {
        int current_final_x = char_start_final_x + font_col_offset;
        int current_final_y = char_start_final_y + font_row_offset;
        setPixel_Final(current_final_x, current_final_y, color);
      }
    }
  }
}

// --- Function to display a two-digit number on the final canvas ---
void displayNumberOnMatrix(int number, CRGB color) {
  FastLED.clearData();

  if (number < 0 || number > 99) return;

  char S1 = (number / 10) + '0';
  char S0 = (number % 10) + '0';

  int spacing_between_digits = 2;
  int total_char_block_width = FONT_CHAR_WIDTH + spacing_between_digits + FONT_CHAR_WIDTH;

  // Center the number on the FINAL visual canvas
  // Note: PANEL_WIDTH and PANEL_HEIGHT now refer to the dimensions of the final rotated view.
  int char1_Start_final_x = (PANEL_WIDTH - total_char_block_width) / 2;
  int char_Start_final_y = (PANEL_HEIGHT - FONT_CHAR_HEIGHT) / 2;
  int char2_Start_final_x = char1_Start_final_x + FONT_CHAR_WIDTH + spacing_between_digits;

  drawCharOnCanvas(S1, char1_Start_final_x, char_Start_final_y, color);
  drawCharOnCanvas(S0, char2_Start_final_x, char_Start_final_y, color);

  FastLED.show();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);

  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  encoder.attachFullQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN);
  encoder.clearCount();

  currentState = IDLE;
  Serial.println("Visual Timer Ready. State: IDLE");
}

void handleButtonPress() {
  switch (currentState) {
    case IDLE:
      currentState = SET_TIME;
      selectedMinutes = 1;
      encoder.setCount(selectedMinutes * 4);
      lastEncoderDetentPosition = selectedMinutes;
      displayNumberOnMatrix(selectedMinutes, CRGB::CornflowerBlue);
      Serial.println("State: SET_TIME");
      break;
    case SET_TIME:
      currentState = RUNNING;
      timerStartTime_ms = millis();
      timerDuration_ms = (unsigned long)selectedMinutes * 60 * 1000;
      FastLED.clearData(); FastLED.show();
      Serial.print("State: RUNNING. Timer set for "); Serial.print(selectedMinutes); Serial.println(" minutes.");
      break;
    case RUNNING:
    case FINISHED:
      currentState = IDLE;
      FastLED.clearData(); FastLED.show();
      Serial.println("State: IDLE (Timer Reset/Cancelled)");
      break;
  }
}

void loop() {
  int currentSwitchState = digitalRead(ENCODER_SW_PIN);
  if (currentSwitchState == LOW && lastSwitchState == HIGH) {
    if (millis() - lastButtonPressTime > DEBOUNCE_DELAY) {
      buttonActionPending = true;
      lastButtonPressTime = millis();
    }
  }
  lastSwitchState = currentSwitchState;

  if (buttonActionPending) {
    handleButtonPress();
    buttonActionPending = false;
  }

  switch (currentState) {
    case IDLE:
      break;
    case SET_TIME: {
      long rawEncoderPos = encoder.getCount();
      long currentDetentValueFromEncoder = rawEncoderPos / 4;
      if (currentDetentValueFromEncoder != lastEncoderDetentPosition) {
        int detentChange = currentDetentValueFromEncoder - lastEncoderDetentPosition;
        selectedMinutes += detentChange;
        selectedMinutes = constrain(selectedMinutes, 1, 90);
        lastEncoderDetentPosition = currentDetentValueFromEncoder;
        if ((selectedMinutes * 4) != rawEncoderPos) {
          encoder.setCount(selectedMinutes * 4);
          lastEncoderDetentPosition = selectedMinutes;
        }
        displayNumberOnMatrix(selectedMinutes, CRGB::CornflowerBlue);
        Serial.print("Time selected: "); Serial.println(selectedMinutes);
      }
      break;
    }
    case RUNNING: {
      unsigned long currentTime_ms = millis();
      unsigned long elapsedTime_ms = currentTime_ms - timerStartTime_ms;
      if (elapsedTime_ms >= timerDuration_ms) {
        currentState = FINISHED;
        Serial.println("State: FINISHED (Timer Complete)");
        FastLED.clearData();
        for (int fy = 0; fy < PANEL_HEIGHT; ++fy) { // final_canvas_y
          for (int fx = 0; fx < PANEL_WIDTH; ++fx) { // final_canvas_x
            setPixel_Final(fx, fy, CHSV(gHue, 255, 255));
          }
        }
        FastLED.show();
      } else {
        float progress = (float)elapsedTime_ms / timerDuration_ms;
        int ledsToLightTarget = progress * NUM_LEDS;
        ledsToLightTarget = constrain(ledsToLightTarget, 0, NUM_LEDS);
        FastLED.clearData();
        int pixels_drawn_count = 0;
        for (int fy = 0; fy < PANEL_HEIGHT; ++fy) { // final_canvas_y
          for (int fx = 0; fx < PANEL_WIDTH; ++fx) { // final_canvas_x
            if (pixels_drawn_count < ledsToLightTarget) {
              setPixel_Final(fx, fy, CHSV(gHue, 255, 255));
              pixels_drawn_count++;
            } else {
              goto end_progress_draw_running;
            }
          }
        }
        end_progress_draw_running:;
        FastLED.show();
        gHue++;
      }
      break;
    }
    case FINISHED:
      FastLED.clearData();
      for (int fy = 0; fy < PANEL_HEIGHT; ++fy) { // final_canvas_y
        for (int fx = 0; fx < PANEL_WIDTH; ++fx) { // final_canvas_x
          setPixel_Final(fx, fy, CHSV(gHue, 255, 255));
        }
      }
      FastLED.show();
      gHue++;
      break;
  }
  delay(20);
}