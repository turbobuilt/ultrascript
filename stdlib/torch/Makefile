# Makefile for torch_c_wrapper
# This compiles the C wrapper into a shared library that UltraScript can use via FFI

CXX = g++
CXXFLAGS = -std=c++17 -fPIC -O3 -Wall -Wextra
TORCH_PATH = ../../../libtorch

# Include directories
INCLUDES = -I$(TORCH_PATH)/include \
           -I$(TORCH_PATH)/include/torch/csrc/api/include

# Library directories and libraries
LDFLAGS = -L$(TORCH_PATH)/lib
LIBS = -ltorch -ltorch_cpu -lc10

# Source and target
SRC = torch_c_wrapper.cpp
TARGET = torch_c_wrapper.so

# Default target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -o $(TARGET) $(SRC) $(LDFLAGS) $(LIBS)

# Test target - compile a simple test program
test: test_wrapper
test_wrapper: test_wrapper.cpp $(TARGET)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o test_wrapper test_wrapper.cpp -L. -ltorch_c_wrapper $(LDFLAGS) $(LIBS)

# Clean target
clean:
	rm -f $(TARGET) test_wrapper

# Install target - copy to a standard location
install: $(TARGET)
	cp $(TARGET) ../../../lib/ 2>/dev/null || mkdir -p ../../../lib && cp $(TARGET) ../../../lib/

# Debug target - compile with debug symbols
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Check if LibTorch is available
check:
	@echo "Checking LibTorch installation..."
	@if [ -d "$(TORCH_PATH)" ]; then \
		echo "✓ LibTorch found at $(TORCH_PATH)"; \
		echo "  Include path: $(TORCH_PATH)/include"; \
		echo "  Library path: $(TORCH_PATH)/lib"; \
		ls -la $(TORCH_PATH)/lib/libtorch*.so | head -3; \
	else \
		echo "✗ LibTorch not found at $(TORCH_PATH)"; \
		echo "  Please ensure LibTorch is installed"; \
		exit 1; \
	fi

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build the torch_c_wrapper.so"
	@echo "  test     - Build and run a test program"
	@echo "  clean    - Remove built files"
	@echo "  install  - Copy wrapper to lib directory"
	@echo "  debug    - Build with debug symbols"
	@echo "  check    - Check LibTorch installation"
	@echo "  help     - Show this help message"

.PHONY: all test clean install debug check help
