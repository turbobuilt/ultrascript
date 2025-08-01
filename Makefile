CXX = g++
CXXFLAGS = -std=c++17 -O0 -Wall -Wextra -pthread
LDFLAGS = -pthread

SRCDIR = .
SOURCES = compiler.cpp lexer.cpp parser.cpp type_inference.cpp x86_codegen.cpp wasm_codegen.cpp ast_codegen.cpp compilation_context.cpp runtime.cpp runtime_syscalls.cpp lexical_scope.cpp regex.cpp error_reporter.cpp syntax_highlighter.cpp simple_main.cpp goroutine_system.cpp function_compilation_manager.cpp goroutine_advanced.cpp runtime_goroutine_advanced.cpp lock_system.cpp lock_jit_integration.cpp runtime_http_server.cpp runtime_http_client.cpp console_log_overhaul.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = ultraScript

.PHONY: all clean debug

all: $(TARGET)

debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

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

# Dependencies
compiler.o: compiler.h runtime.h
lexer.o: compiler.h
parser.o: compiler.h
type_inference.o: compiler.h
x86_codegen.o: compiler.h
wasm_codegen.o: compiler.h
ast_codegen.o: compiler.h runtime_object.h compilation_context.h
compilation_context.o: compilation_context.h compiler.h
runtime.o: runtime.h lexical_scope.h
runtime_syscalls.o: runtime_syscalls.h runtime.h runtime_object.h lock_system.h
lock_system.o: lock_system.h goroutine_system.h
lexical_scope.o: lexical_scope.h compiler.h
regex.o: regex.h runtime.h
error_reporter.o: compiler.h
syntax_highlighter.o: compiler.h
main.o: compiler.h tensor.h promise.h runtime.h