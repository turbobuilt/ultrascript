#!/bin/bash

echo "=== UltraScript Working Benchmark ==="
echo ""

echo "Running Node.js Fibonacci benchmark..."
/usr/bin/time -f "Node.js time: %es" node node_test.js
echo ""

echo "Running UltraScript benchmark (with working goroutines)..."
/usr/bin/time -f "UltraScript goroutine time: %es" ../ultraScript working_multiecma_test.gts
echo ""

echo "Running UltraScript sequential benchmark (working version)..."
/usr/bin/time -f "UltraScript sequential time: %es" ../ultraScript working_multiecma_sequential.gts
echo ""

echo "=== Benchmark Comparison Complete ==="