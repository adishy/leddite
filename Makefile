# Leddite V2 Makefile
# Supports native unit tests and WebAssembly simulator build

# --- Configuration ---
CXX = g++
EMCC = emcc
CXXFLAGS = -O3 -Iinclude
WASM_FLAGS = --bind -s WASM=1 -s ALLOW_MEMORY_GROWTH=1

# Source files
CORE_SRCS = src/Canvas.cpp src/Transformer.cpp src/MarqueeEngine.cpp src/ProtocolHandler.cpp
WASM_BRIDGE = simulator/wasm_bridge.cpp
SIM_DIR = simulator

# Test files
TEST_SRCS = test/test_canvas.cpp test/test_transformer.cpp test/test_protocol.cpp
TEST_BINS = test/test_canvas test/test_transformer test/test_protocol

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

test/test_canvas: test/test_canvas.cpp src/Canvas.cpp src/Transformer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_transformer: test/test_transformer.cpp src/Transformer.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test/test_protocol: test/test_protocol.cpp src/ProtocolHandler.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

# Convenience target to start the simulator server
run-sim: simulator
	@echo "Starting simulator server..."
	./.venv/bin/python simulator_server.py

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
