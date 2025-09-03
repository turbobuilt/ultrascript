CXX = g++
CXXFLAGS = -std=c++17 -O0 -Wall -Wextra -pthread
LDFLAGS = -pthread -ldl

SRCDIR = .
SOURCES = compiler.cpp lexer.cpp parser.cpp minimal_parser_gc.cpp gc_system.cpp x86_instruction_builder.cpp x86_pattern_builder.cpp x86_codegen_v2.cpp ast_codegen.cpp function_runtime.cpp function_codegen.cpp compilation_context.cpp runtime.cpp runtime_syscalls.cpp regex.cpp error_reporter.cpp syntax_highlighter.cpp simple_main.cpp goroutine_system_v2.cpp function_compilation_manager.cpp lock_system.cpp lock_jit_integration.cpp runtime_http_server.cpp runtime_http_client.cpp console_log_overhaul.cpp ffi_syscalls.cpp free_runtime.cpp dynamic_properties.cpp simple_lexical_scope.cpp lexical_scope_node.cpp type_inference_stub.cpp function_address_patching.cpp scope_aware_codegen.cpp static_analyzer_clean.cpp
ASM_SOURCES = context_switch.s
OBJECTS = $(SOURCES:.cpp=.o) $(ASM_SOURCES:.s=.o)
TARGET = ultraScript

.PHONY: all clean debug

all: $(TARGET)

debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.s
	$(CXX) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: x86-test wasm-test

x86-test: $(TARGET)
	@echo "Testing x86-64 backend..."
	./$(TARGET)

wasm-test: $(TARGET)
	@echo "Testing WebAssembly backend..."
	./$(TARGET)

benchmark: $(TARGET)
	@echo "Running performance benchmarks..."
	time ./$(TARGET)

# FFI Tests
test-ffi: test_ffi
	@echo "Running FFI tests..."
	./test_ffi

test_ffi: test_ffi.cpp ffi_syscalls.o
	$(CXX) $(CXXFLAGS) test_ffi.cpp ffi_syscalls.o -o test_ffi $(LDFLAGS)

# Dependencies
compiler.o: compiler.h runtime.h
lexer.o: compiler.h
parser.o: compiler.h
type_inference.o: compiler.h
x86_codegen.o: compiler.h
wasm_codegen.o: compiler.h
ast_codegen.o: compiler.h runtime_object.h compilation_context.h
compilation_context.o: compilation_context.h compiler.h
runtime.o: runtime.h
# Removed lexical_scope.h dependency - using pure static analysis now
runtime_syscalls.o: runtime_syscalls.h runtime.h runtime_object.h lock_system.h
lock_system.o: lock_system.h goroutine_system_v2.h
goroutine_system_v2.o: goroutine_system_v2.h
context_switch.o: 
# Removed lexical_scope.o rule - using pure static analysis now
regex.o: regex.h runtime.h
error_reporter.o: compiler.h
syntax_highlighter.o: compiler.h
main.o: compiler.h tensor.h promise.h runtime.h
ffi_syscalls.o: ffi_syscalls.h