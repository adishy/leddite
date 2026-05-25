# Leddite V2 Makefile
# Supports native unit tests and WebAssembly simulator build

# --- Configuration ---
CXX = g++
EMCC = emcc
CXXFLAGS = -O3 -Iinclude
# --bind  : enables emscripten::bind (exposes Canvas class to JS as Module.Canvas)
# IMPORTANT: wasm_bridge.cpp MUST be included in every build or the Canvas
#            bindings will be silently absent and the simulator will render nothing.
WASM_FLAGS = --bind -s WASM=1 -s ALLOW_MEMORY_GROWTH=1

# Source files
CORE_SRCS = src/Canvas.cpp src/Transformer.cpp src/MarqueeEngine.cpp src/ProtocolHandler.cpp src/TextRenderer.cpp
WASM_BRIDGE = simulator/wasm_bridge.cpp
SIM_DIR = simulator

# Test files
TEST_SRCS = test/test_canvas.cpp test/test_transformer.cpp test/test_protocol.cpp test/test_text_renderer.cpp
TEST_BINS = test/test_canvas test/test_transformer test/test_protocol test/test_text_renderer

# --- Targets ---

.PHONY: all clean test simulator check-wasm run-sim help

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
missing = [k for k in [b'Canvas',b'drawSprite',b'getBuffer',b'stopMarquee'] if k not in data]; \
sys.exit(0) if not missing else (print('WASM check FAILED — bindings missing from .wasm: ' + str([m.decode() for m in missing])), sys.exit(1)) \
" && echo "WASM binding check passed (Canvas bindings present in .wasm)" \
  || (echo ""; echo "  Run: make simulator   (requires emcc in PATH)"; echo ""; exit 1)

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
	@echo "  make run-sim     - Build WASM and start the Python simulator server"
	@echo "  make clean       - Remove build artifacts"
