#!/bin/bash

# Build script for the JavaScript validation tests
# This includes the minimal set of object files needed

echo "🔨 Building JavaScript validation tests..."

# First, make sure all object files are built
echo "Step 1: Building all UltraScript objects..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "❌ Failed to build UltraScript objects"
    exit 1
fi

echo "✅ UltraScript objects built successfully"

# Define the core object files we need for JavaScript parsing and static analysis
CORE_OBJECTS="
    static_scope_analyzer.o
    ast_codegen.o
    parser.o
    lexer.o
    compiler.o
    type_inference.o
    error_reporter.o
    syntax_highlighter.o
    compilation_context.o
    simple_lexical_scope.o
"

# Optional objects that might be needed for full functionality
OPTIONAL_OBJECTS="
    minimal_parser_gc.o
    function_compilation_manager.o
"

echo "Step 2: Building JavaScript validation test..."

g++ -std=c++17 -I. \
    test_real_js_validation.cpp \
    $CORE_OBJECTS \
    $OPTIONAL_OBJECTS \
    -o test_real_js_validation \
    -pthread

if [ $? -eq 0 ]; then
    echo "✅ test_real_js_validation built successfully"
    echo "🚀 Running test..."
    ./test_real_js_validation
else
    echo "❌ Failed to build test_real_js_validation"
    echo "Trying with minimal objects only..."
    
    # Try with just the essential ones
    g++ -std=c++17 -I. \
        ultrascript_integration_test.cpp \
        static_scope_analyzer.o \
        -o ultrascript_integration_test \
        -pthread
    
    if [ $? -eq 0 ]; then
        echo "✅ ultrascript_integration_test built successfully"
        echo "🚀 Running minimal test..."
        ./ultrascript_integration_test
    else
        echo "❌ Even minimal test failed to build"
        echo "Let's check what's missing..."
        nm -C static_scope_analyzer.o | grep "U " | head -10
    fi
fi

echo "Step 3: Building simple JavaScript validation demo..."

g++ -std=c++17 \
    simple_js_validation.cpp \
    -o simple_js_validation

if [ $? -eq 0 ]; then
    echo "✅ simple_js_validation built successfully"
    echo "🚀 Running simple demo..."
    ./simple_js_validation
else
    echo "❌ Failed to build simple_js_validation"
fi

echo "🎯 Build and test process complete!"
