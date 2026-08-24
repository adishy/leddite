# Leddite V2 Makefile
# Supports native unit tests and WebAssembly simulator build

# --- Configuration ---
CXX = g++
EMCC = emcc
CXXFLAGS = -O3 -Iinclude
# --bind  : enables emscripten::bind (exposes Canvas class to JS as Module.Canvas)
# IMPORTANT: wasm_bridge.cpp MUST be included in every build or the Canvas
#            bindings will be silently absent and the simulator will render nothing.
# --bind        : enables emscripten::bind (exposes Canvas/DeviceUI to JS)
# MODULARIZE    : emits a factory instead of assigning a global `Module`. This
#                 lets node require() the glue for `make test-wasm`, and removes
#                 the fragile `var Module = {}` pre-declaration that index.html
#                 previously needed (a missing one blacked out the simulator).
WASM_FLAGS = --bind -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 \
             -s MODULARIZE=1 -s EXPORT_NAME=createLedditeModule \
             -s ENVIRONMENT=web,node

# Source files
# PROTOCOL_SRCS: the original binary-protocol core.
# MODE_SRCS:     Arduino-free mode logic (see docs/adr/0009) — games, menus,
#                settings and weather. Unit-tested natively and compiled into
#                the WASM module so the simulator runs the same code as the ESP32.
PROTOCOL_SRCS = src/Canvas.cpp src/Transformer.cpp src/MarqueeEngine.cpp src/ProtocolHandler.cpp src/TextRenderer.cpp
MODE_SRCS     = src/ColorUtils.cpp src/Draw.cpp src/SmallTextRenderer.cpp src/ListMenu.cpp \
                src/GameEngine.cpp src/BrightnessModel.cpp src/WeatherView.cpp src/Places.cpp \
                src/UiController.cpp
CORE_SRCS     = $(PROTOCOL_SRCS) $(MODE_SRCS)
WASM_BRIDGE = simulator/wasm_bridge.cpp
SIM_DIR = simulator

# Test files
TEST_BINS = test/test_canvas test/test_transformer test/test_protocol test/test_text_renderer \
            test/test_color_utils test/test_small_font test/test_list_menu \
            test/test_game_engine test/test_brightness test/test_weather_view

# --- Targets ---

.PHONY: all clean test test-wasm simulator check-wasm check-artifacts run-sim help

all: test simulator

# Build the WASM module for the JS simulator, then verify the bindings compiled in.
# Rebuild triggers: any change to src/, include/, or simulator/wasm_bridge.cpp.
simulator: $(CORE_SRCS) $(WASM_BRIDGE)
	@echo "Building WebAssembly module..."
	$(EMCC) $(CXXFLAGS) $(WASM_FLAGS) $(CORE_SRCS) $(WASM_BRIDGE) -o $(SIM_DIR)/leddite_wasm.js
	@echo "WASM build complete: $(SIM_DIR)/leddite_wasm.js"
	@$(MAKE) --no-print-directory check-wasm

# Verify the compiled WASM binary contains the embind Canvas bindings.
# With -O3, Emscripten embeds binding string literals in the .wasm binary (not
# in the JS glue), so check-wasm scans leddite_wasm.wasm as raw bytes.
# If this fails the WASM was compiled without wasm_bridge.cpp and the simulator
# will display nothing (ledditeCanvas stays null, all handleBinary calls dropped).
check-wasm:
	@python3 -c "\
import sys; \
data = open('$(SIM_DIR)/leddite_wasm.wasm','rb').read(); \
missing = [k for k in [b'Canvas',b'drawSprite',b'getBuffer',b'stopMarquee',b'DeviceUI',b'longPress',b'enterSettings'] if k not in data]; \
sys.exit(0) if not missing else (print('WASM check FAILED — bindings missing from .wasm: ' + str([m.decode() for m in missing])), sys.exit(1)) \
" && echo "WASM binding check passed (Canvas bindings present in .wasm)" \
  || (echo ""; echo "  Run: make simulator   (requires emcc in PATH)"; echo ""; exit 1)

# Drive the COMMITTED leddite_wasm.js through its DeviceUI binding under node.
# Native unit tests rebuild src/ and so cannot see defects in the artifact the
# browser loads — which is exactly how the simulator went black twice.
test-wasm:
	@node $(SIM_DIR)/test_device_ui.mjs

# Fail if any tracked file is a compiled binary (RULES.md section 1)
check-artifacts:
	@tools/check-no-binaries.sh

# Build and run native unit tests
test: $(TEST_BINS)
	@echo "Running unit tests..."
	@for test in $(TEST_BINS); do ./$$test; done

test/test_canvas: test/test_canvas.cpp src/Canvas.cpp src/Transformer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_transformer: test/test_transformer.cpp src/Transformer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_protocol: test/test_protocol.cpp src/ProtocolHandler.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_text_renderer: test/test_text_renderer.cpp src/TextRenderer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

# Mode-logic tests (docs/adr/0009)
test/test_color_utils: test/test_color_utils.cpp src/ColorUtils.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_small_font: test/test_small_font.cpp src/SmallTextRenderer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_list_menu: test/test_list_menu.cpp src/ListMenu.cpp src/Draw.cpp src/SmallTextRenderer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_game_engine: test/test_game_engine.cpp src/GameEngine.cpp src/Draw.cpp src/ColorUtils.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_brightness: test/test_brightness.cpp src/BrightnessModel.cpp src/Draw.cpp src/SmallTextRenderer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

# TextRenderer is the 5x7 font the condition description is drawn in.
test/test_weather_view: test/test_weather_view.cpp src/WeatherView.cpp src/Draw.cpp src/SmallTextRenderer.cpp src/TextRenderer.cpp src/Places.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

# Convenience target to start the simulator server
run-sim: simulator
	@echo "Starting simulator server..."
	./.venv/bin/python simulator_server.py

clean:
	rm -f $(TEST_BINS)
	rm -f $(SIM_DIR)/leddite_wasm.js $(SIM_DIR)/leddite_wasm.wasm
	@echo "Clean complete."

# Individual test targets for convenience
test-text-renderer: test/test_text_renderer
	./test/test_text_renderer

help:
	@echo "Leddite V2 Build System"
	@echo "Targets:"
	@echo "  make test        - Build and run native C++ unit tests"
	@echo "  make simulator   - Build the WebAssembly module for the browser + verify bindings"
	@echo "  make check-wasm  - Verify committed WASM has Canvas bindings (no rebuild)"
	@echo "  make test-wasm   - Drive the committed WASM DeviceUI binding under node"
	@echo "  make check-artifacts - Fail if any compiled binary is tracked in git"
	@echo "  make run-sim     - Build WASM and start the Python simulator server"
	@echo "  make clean       - Remove build artifacts"
