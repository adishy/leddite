#include <FastLED.h>
#include <ESP32Encoder.h>

// --- LED Panel Configuration ---
#define LED_DATA_PIN   4
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define LED_ROWS       16
#define LED_COLS       16
#define NUM_LEDS       256 // 16x16
#define BRIGHTNESS     50
const int PANEL_WIDTH  = 16; // Number of columns
const int PANEL_HEIGHT = 16; // Number of rows
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
const unsigned long DEBOUNCE_DELAY = 100;
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

// --- XY Mapping Function (Matches Python's set_pixel ordering) ---
// Takes x_row (row index) and y_col (column index)
uint16_t XY(uint8_t x_col, uint8_t y_row) {
  // x_col is the column index (0 to LED_COLUMNS-1)
  // y_row is the row index (0 to LED_ROWS-1)

  if (x_col >= LED_COLS || y_row >= LED_ROWS) {
    return NUM_LEDS; // Return an out-of-bounds index for invalid coordinates
  }

  uint16_t index;
  uint8_t effective_col = x_col;

  // This matches Python's: if (row % 2) > 0 (i.e., row is ODD)
  if ((y_row % 2) != 0) {
    // Row is ODD, so flip the column index
    // Python: y = self.LED_ROWS - 1 - y  (where self.LED_ROWS is panel width, 16)
    // C++: effective_col = (width_of_panel - 1) - original_column_index
    effective_col = (LED_COLS - 1) - x_col;
  }
  // Else (row is EVEN), effective_col remains x_col (original column index)

  // Python: position_in_grid = x * self.LED_ROWS + y
  // C++: index = y_row * panel_width + effective_col
  index = (y_row * LED_COLS) + effective_col;

  return index;
}

// --- Function to draw a character ---
// char_origin_col, char_origin_row: top-left corner of the character on the panel
void drawChar(char c, int char_origin_col, int char_origin_row, CRGB color) {
  if (c < '0' || c > '9') return;
  uint8_t digitIndex = c - '0';

  for (int char_col_offset = 0; char_col_offset < FONT_CHAR_WIDTH; char_col_offset++) {
    uint8_t font_column_data = font5x7_data[digitIndex][char_col_offset];
    for (int char_row_offset = 0; char_row_offset < FONT_CHAR_HEIGHT; char_row_offset++) {
      if ((font_column_data >> char_row_offset) & 0x01) {
        int panel_row = char_origin_row + char_row_offset;
        int panel_col = char_origin_col + char_col_offset;
        uint16_t ledIndex = XY(panel_row, panel_col); // Call with (row, column)
        if (ledIndex < NUM_LEDS) {
          leds[ledIndex] = color;
        }
      }
    }
  }
}

// --- Function to display a two-digit number ---
void displayNumberOnMatrix(int number, CRGB color) {
  FastLED.clearData();

  if (number < 0 || number > 99) return;

  char S1 = (number / 10) + '0'; // Tens digit
  char S0 = (number % 10) + '0'; // Ones digit

  int spacing_between_digits = 2;
  int total_char_block_width = FONT_CHAR_WIDTH + spacing_between_digits + FONT_CHAR_WIDTH;

  // Center the number on the panel
  int char1_origin_col = (PANEL_WIDTH - total_char_block_width) / 2;
  int char_origin_row = (PANEL_HEIGHT - FONT_CHAR_HEIGHT) / 2;
  int char2_origin_col = char1_origin_col + FONT_CHAR_WIDTH + spacing_between_digits;

  drawChar(S1, char1_origin_col, char_origin_row, color);
  drawChar(S0, char2_origin_col, char_origin_row, color);

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
        for (int r = 0; r < PANEL_HEIGHT; ++r) {
          for (int c = 0; c < PANEL_WIDTH; ++c) {
            uint16_t idx = XY(r, c); // Call with (row, column)
            if(idx < NUM_LEDS) leds[idx] = CHSV(gHue, 255, 255);
          }
        }
        FastLED.show();
      } else {
        float progress = (float)elapsedTime_ms / timerDuration_ms;
        int ledsToLightTarget = progress * NUM_LEDS;
        ledsToLightTarget = constrain(ledsToLightTarget, 0, NUM_LEDS);
        FastLED.clearData();
        int pixels_drawn_count = 0;
        for (int r = 0; r < PANEL_HEIGHT; ++r) {
          for (int c = 0; c < PANEL_WIDTH; ++c) {
            if (pixels_drawn_count < ledsToLightTarget) {
              uint16_t idx = XY(r, c); // Call with (row, column)
              if(idx < NUM_LEDS) leds[idx] = CHSV(gHue, 255, 255);
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
      for (int r = 0; r < PANEL_HEIGHT; ++r) {
        for (int c = 0; c < PANEL_WIDTH; ++c) {
           uint16_t idx = XY(r, c); // Call with (row, column)
           if(idx < NUM_LEDS) leds[idx] = CHSV(gHue, 255, 255);
        }
      }
      FastLED.show();
      gHue++;
      break;
  }
  delay(20);
}