#!/bin/bash

# ============================================================================
# BUILD SCRIPT FOR ULTRASCRIPT REFERENCE COUNTING SYSTEM
# ============================================================================

echo "=== Building UltraScript Reference Counting System ==="

# Compiler settings for maximum performance
CXX="g++"
CXXFLAGS="-std=c++17 -O3 -march=native -mtune=native"
CXXFLAGS="$CXXFLAGS -ffast-math -funroll-loops -flto"
CXXFLAGS="$CXXFLAGS -DREFCOUNT_USE_INTRINSICS=1"
CXXFLAGS="$CXXFLAGS -DREFCOUNT_CACHE_ALIGNED=1"
CXXFLAGS="$CXXFLAGS -DREFCOUNT_WEAK_REFS=1"
CXXFLAGS="$CXXFLAGS -DREFCOUNT_THREAD_SAFE=1"

# Debug vs Release
if [[ "$1" == "debug" ]]; then
    echo "Building DEBUG version..."
    CXXFLAGS="$CXXFLAGS -g -O1 -DREFCOUNT_DEBUG_MODE=1"
    BUILD_DIR="build_debug"
else
    echo "Building RELEASE version..."
    CXXFLAGS="$CXXFLAGS -DNDEBUG -DREFCOUNT_DEBUG_MODE=0"
    BUILD_DIR="build_release"
fi

# Threading support
CXXFLAGS="$CXXFLAGS -pthread"
LDFLAGS="-pthread"

# Create build directory
mkdir -p $BUILD_DIR
cd $BUILD_DIR

echo "Compiler: $CXX"
echo "Flags: $CXXFLAGS"
echo ""

# Compile reference counting core
echo "Compiling reference counting core..."
$CXX $CXXFLAGS -c ../refcount.cpp -o refcount.o
if [[ $? -ne 0 ]]; then
    echo "Failed to compile refcount.cpp"
    exit 1
fi

# Compile assembly generation
echo "Compiling assembly generation..."
$CXX $CXXFLAGS -c ../refcount_asm.cpp -o refcount_asm.o
if [[ $? -ne 0 ]]; then
    echo "Failed to compile refcount_asm.cpp"
    exit 1
fi

# Compile free runtime integration
echo "Compiling free runtime integration..."
$CXX $CXXFLAGS -c ../free_runtime.cpp -o free_runtime.o
if [[ $? -ne 0 ]]; then
    echo "Failed to compile free_runtime.cpp"
    exit 1
fi

# Compile atomic refcount compatibility
echo "Compiling atomic refcount compatibility..."
$CXX $CXXFLAGS -c ../atomic_refcount.cpp -o atomic_refcount.o
if [[ $? -ne 0 ]]; then
    echo "Failed to compile atomic_refcount.cpp"
    exit 1
fi

# Create static library
echo "Creating static library..."
ar rcs librefcount.a refcount.o refcount_asm.o free_runtime.o atomic_refcount.o
if [[ $? -ne 0 ]]; then
    echo "Failed to create static library"
    exit 1
fi

# Compile test program
echo "Compiling test program..."
$CXX $CXXFLAGS ../test_refcount_system.cpp -L. -lrefcount $LDFLAGS -o test_refcount
if [[ $? -ne 0 ]]; then
    echo "Failed to compile test program"
    exit 1
fi

# Create shared library for dynamic linking
echo "Creating shared library..."
$CXX $CXXFLAGS -shared -fPIC refcount.o refcount_asm.o free_runtime.o atomic_refcount.o $LDFLAGS -o librefcount.so
if [[ $? -ne 0 ]]; then
    echo "Failed to create shared library"
    exit 1
fi

echo ""
echo "=== Build Complete ==="
echo "Static library: $BUILD_DIR/librefcount.a"
echo "Shared library: $BUILD_DIR/librefcount.so"
echo "Test program: $BUILD_DIR/test_refcount"
echo ""

# Run tests if requested
if [[ "$2" == "test" ]]; then
    echo "=== Running Tests ==="
    ./test_refcount
    echo ""
fi

# Generate assembly demonstration if requested
if [[ "$2" == "asm" || "$3" == "asm" ]]; then
    echo "=== Generating Assembly Demonstration ==="
    echo "Creating assembly demo program..."
    
    cat > ../demo_assembly.cpp << 'EOF'
#include "refcount_asm.h"
#include <iostream>

// Forward declaration for demo
void demonstrate_generated_assembly();

int main() {
    std::cout << "=== ULTRASCRIPT REFERENCE COUNTING ASSEMBLY GENERATION ===" << std::endl;
    demonstrate_generated_assembly();
    return 0;
}
EOF

    $CXX $CXXFLAGS ../demo_assembly.cpp -L. -lrefcount $LDFLAGS -o demo_assembly
    if [[ $? -eq 0 ]]; then
        echo "Running assembly demonstration..."
        ./demo_assembly
    else
        echo "Failed to compile assembly demo"
    fi
    echo ""
fi

# Performance benchmark if requested
if [[ "$2" == "bench" || "$3" == "bench" ]]; then
    echo "=== Performance Benchmark ==="
    echo "Creating benchmark program..."
    
    cat > ../benchmark_refcount.cpp << 'EOF'
#include "refcount.h"
#include <chrono>
#include <iostream>
#include <vector>

int main() {
    const int NUM_OBJECTS = 100000;
    const int NUM_ITERATIONS = 10;
    
    std::cout << "=== REFERENCE COUNTING PERFORMANCE BENCHMARK ===" << std::endl;
    std::cout << "Objects: " << NUM_OBJECTS << ", Iterations: " << NUM_ITERATIONS << std::endl;
    
    double total_alloc_time = 0;
    double total_retain_time = 0;
    double total_release_time = 0;
    
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        std::vector<void*> objects;
        objects.reserve(NUM_OBJECTS);
        
        // Allocation benchmark
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_OBJECTS; ++i) {
            void* obj = rc_alloc(64, 0, nullptr);
            objects.push_back(obj);
        }
        auto end = std::chrono::high_resolution_clock::now();
        total_alloc_time += std::chrono::duration<double, std::micro>(end - start).count();
        
        // Retain benchmark
        start = std::chrono::high_resolution_clock::now();
        for (void* obj : objects) {
            rc_retain(obj);
        }
        end = std::chrono::high_resolution_clock::now();
        total_retain_time += std::chrono::duration<double, std::micro>(end - start).count();
        
        // Release benchmark (2x for the retain + original)
        start = std::chrono::high_resolution_clock::now();
        for (void* obj : objects) {
            rc_release(obj);  // Release retain
            rc_release(obj);  // Release original
        }
        end = std::chrono::high_resolution_clock::now();
        total_release_time += std::chrono::duration<double, std::micro>(end - start).count();
    }
    
    std::cout << "\nAverage Performance (per operation):" << std::endl;
    std::cout << "Allocation: " << (total_alloc_time / NUM_ITERATIONS / NUM_OBJECTS) << " μs" << std::endl;
    std::cout << "Retain:     " << (total_retain_time / NUM_ITERATIONS / NUM_OBJECTS) << " μs" << std::endl;
    std::cout << "Release:    " << (total_release_time / NUM_ITERATIONS / NUM_OBJECTS / 2) << " μs" << std::endl;
    
    return 0;
}
EOF

    $CXX $CXXFLAGS ../benchmark_refcount.cpp -L. -lrefcount $LDFLAGS -o benchmark_refcount
    if [[ $? -eq 0 ]]; then
        echo "Running performance benchmark..."
        ./benchmark_refcount
    else
        echo "Failed to compile benchmark"
    fi
    echo ""
fi

echo "=== Build Script Complete ==="
echo ""
echo "Usage examples:"
echo "  ./build_refcount.sh           # Release build"
echo "  ./build_refcount.sh debug     # Debug build"
echo "  ./build_refcount.sh release test  # Release build + run tests"
echo "  ./build_refcount.sh debug asm     # Debug build + show assembly"
echo "  ./build_refcount.sh release bench # Release build + benchmark"
echo ""
echo "Integration:"
echo "  # Static linking"
echo "  g++ your_code.cpp -L$BUILD_DIR -lrefcount -pthread"
echo ""
echo "  # Dynamic linking"
echo "  g++ your_code.cpp -L$BUILD_DIR -lrefcount -pthread"
echo "  export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$(pwd)/$BUILD_DIR"
