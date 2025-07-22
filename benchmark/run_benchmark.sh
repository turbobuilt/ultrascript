#!/bin/bash

echo "=== UltraScript Benchmark ==="
echo ""

echo "Running Node.js Fibonacci benchmark..."
/usr/bin/time -f "Node.js time: %es" node node_test.js
echo ""

echo "Running UltraScript benchmark (with goroutines)..."
/usr/bin/time -f "UltraScript time: %es" ../ultraScript multiecma_test.gts
echo ""

echo "Running UltraScript sequential benchmark..."
/usr/bin/time -f "UltraScript sequential time: %es" ../ultraScript multiecma_sequential.gts
