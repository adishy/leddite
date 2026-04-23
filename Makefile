# Leddite V2 Makefile
# Supports native unit tests and WebAssembly simulator build

# --- Configuration ---
CXX = g++
EMCC = emcc
CXXFLAGS = -O3 -Iv2/include
WASM_FLAGS = --bind -s WASM=1 -s ALLOW_MEMORY_GROWTH=1

# Source files
CORE_SRCS = v2/src/Canvas.cpp v2/src/Transformer.cpp v2/src/MarqueeEngine.cpp v2/src/ProtocolHandler.cpp
WASM_BRIDGE = v2/simulator/wasm_bridge.cpp
SIM_DIR = v2/simulator

# Test files
TEST_SRCS = v2/test/test_canvas.cpp v2/test/test_transformer.cpp v2/test/test_protocol.cpp
TEST_BINS = v2/test/test_canvas v2/test/test_transformer v2/test/test_protocol

# --- Targets ---

.PHONY: all clean test simulator run-sim help

all: test simulator

# Build the WASM module for the JS simulator
simulator: $(CORE_SRCS) $(WASM_BRIDGE)
	@echo "Building WebAssembly module..."
	$(EMCC) $(CXXFLAGS) $(WASM_FLAGS) $(CORE_SRCS) $(WASM_BRIDGE) -o $(SIM_DIR)/leddite_wasm.js
	@echo "WASM build complete: $(SIM_DIR)/leddite_wasm.js"

# Build and run native unit tests
test: $(TEST_BINS)
	@echo "Running unit tests..."
	@for test in $(TEST_BINS); do ./$$test; done

v2/test/test_canvas: v2/test/test_canvas.cpp v2/src/Canvas.cpp v2/src/Transformer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

v2/test/test_transformer: v2/test/test_transformer.cpp v2/src/Transformer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

v2/test/test_protocol: v2/test/test_protocol.cpp v2/src/ProtocolHandler.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

# Convenience target to start the simulator server
run-sim: simulator
	@echo "Starting simulator server..."
	./v2/.venv/bin/python v2/simulator_server.py

clean:
	rm -f $(TEST_BINS)
	rm -f $(SIM_DIR)/leddite_wasm.js $(SIM_DIR)/leddite_wasm.wasm
	@echo "Clean complete."

help:
	@echo "Leddite V2 Build System"
	@echo "Targets:"
	@echo "  make test      - Build and run native C++ unit tests"
	@echo "  make simulator - Build the WebAssembly module for the browser"
	@echo "  make run-sim   - Build WASM and start the Python simulator server"
	@echo "  make clean     - Remove build artifacts"
