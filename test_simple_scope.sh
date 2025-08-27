#!/bin/bash

echo "Testing Simple Lexical Scope System..."
echo "======================================"

# Build the project first
echo "Building project..."
if ! bash build.sh; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "Testing simple lexical scope with test_simple_lexical_scope.gts..."
echo ""

# Run the test
if [[ -f "./ultrascript" ]]; then
    ./ultrascript test_simple_lexical_scope.gts
    echo ""
    echo "Test completed."
else
    echo "UltraScript executable not found!"
    exit 1
fi
