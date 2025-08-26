#!/bin/bash

# UltraScript ES6 Scoping Test Suite Build & Run Script
# This script builds and runs comprehensive ES6 block scoping tests

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
COMPILER_FLAGS="-std=c++17 -I. -pthread -ldl"
BUILD_DIR="."
TEST_PREFIX="test_"

# All object file dependencies for UltraScript tests
DEPENDENCIES=(
    "ast_codegen.o"
    "compilation_context.o" 
    "compiler.o"
    "console_log_overhaul.o"
    "dynamic_properties.o"
    "error_reporter.o"
    "escape_analyzer.o"
    "ffi_syscalls.o"
    "free_runtime.o"
    "function_compilation_manager.o"
    "gc_system.o"
    "goroutine_system_v2.o"
    "lexer.o"
    "lexical_scope_address_tracker.o"
    "lexical_scope_layout.o"
    "lock_jit_integration.o"
    "lock_system.o"
    "minimal_parser_gc.o"
    "parser.o"
    "regex.o"
    "runtime.o"
    "runtime_syscalls.o"
    "static_scope_analyzer.o"
    "syntax_highlighter.o"
    "type_inference.o"
    "x86_codegen_v2.o"
    "x86_instruction_builder.o"
    "x86_pattern_builder.o"
    "runtime_http_client.o"
    "runtime_http_server.o"
    "context_switch.o"
)

# Function to print colored output
print_color() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

# Function to check if all dependencies exist
check_dependencies() {
    print_color $BLUE "🔍 Checking UltraScript dependencies..."
    
    local missing_deps=()
    for dep in "${DEPENDENCIES[@]}"; do
        if [ ! -f "$dep" ]; then
            missing_deps+=("$dep")
        fi
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_color $RED "❌ Missing dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "   - $dep"
        done
        print_color $YELLOW "💡 Run 'bash build.sh' first to build UltraScript"
        return 1
    fi
    
    print_color $GREEN "✅ All dependencies found (${#DEPENDENCIES[@]} files)"
    return 0
}

# Function to build a test
build_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .cpp)
    local executable="${test_name}"
    
    print_color $CYAN "🔨 Building $test_name..."
    
    # Create the full command with all dependencies
    local cmd="g++ $COMPILER_FLAGS -o $executable $test_file ${DEPENDENCIES[*]}"
    
    if $cmd; then
        print_color $GREEN "✅ Built $executable successfully"
        return 0
    else
        print_color $RED "❌ Failed to build $test_name"
        return 1
    fi
}

# Function to run a test
run_test() {
    local executable=$1
    local test_name=$1
    
    if [ ! -f "$executable" ]; then
        print_color $RED "❌ Executable $executable not found"
        return 1
    fi
    
    print_color $PURPLE "🚀 Running $test_name..."
    echo ""
    
    # Run the test and capture exit code
    if ./$executable; then
        print_color $GREEN "✅ $test_name completed successfully"
        return 0
    else
        local exit_code=$?
        print_color $RED "❌ $test_name failed with exit code $exit_code"
        return $exit_code
    fi
}

# Function to build and run a test
build_and_run_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .cpp)
    
    print_color $BLUE "================================================="
    print_color $BLUE "Processing: $test_name"
    print_color $BLUE "================================================="
    
    if build_test "$test_file"; then
        echo ""
        run_test "$test_name"
        return $?
    else
        return 1
    fi
}

# Function to clean built executables
clean_tests() {
    print_color $YELLOW "🧹 Cleaning test executables..."
    
    local cleaned=0
    for test_file in test_*.cpp; do
        if [ -f "$test_file" ]; then
            local test_name=$(basename "$test_file" .cpp)
            if [ -f "$test_name" ]; then
                rm "$test_name"
                echo "   Removed $test_name"
                cleaned=$((cleaned + 1))
            fi
        fi
    done
    
    print_color $GREEN "✅ Cleaned $cleaned test executables"
}

# Function to list available tests
list_tests() {
    print_color $BLUE "📋 Available ES6 Scoping Tests:"
    echo ""
    
    local count=0
    for test_file in test_*.cpp; do
        if [ -f "$test_file" ]; then
            local test_name=$(basename "$test_file" .cpp)
            local description=""
            
            # Try to extract description from file
            if grep -q "ULTRA COMPLEX" "$test_file" 2>/dev/null; then
                description="(Ultra-complex nested scoping test)"
            elif grep -q "Raw JavaScript" "$test_file" 2>/dev/null; then
                description="(Raw JavaScript ES6 validation)"
            elif grep -q "while.*loop" "$test_file" 2>/dev/null; then
                description="(While loop implementation test)"
            else
                description="(ES6 scoping test)"
            fi
            
            printf "   %-35s %s\n" "$test_name" "$description"
            count=$((count + 1))
        fi
    done
    
    if [ $count -eq 0 ]; then
        print_color $YELLOW "   No test files found (test_*.cpp)"
    else
        echo ""
        print_color $GREEN "Found $count test files"
    fi
}

# Function to run all tests
run_all_tests() {
    print_color $BLUE "🎯 Running ALL ES6 Scoping Tests"
    print_color $BLUE "================================="
    
    local total=0
    local passed=0
    local failed=0
    
    for test_file in test_*.cpp; do
        if [ -f "$test_file" ]; then
            total=$((total + 1))
            echo ""
            
            if build_and_run_test "$test_file"; then
                passed=$((passed + 1))
            else
                failed=$((failed + 1))
            fi
        fi
    done
    
    # Final summary
    echo ""
    print_color $BLUE "================================================="
    print_color $BLUE "FINAL TEST SUITE RESULTS"
    print_color $BLUE "================================================="
    echo "Total tests: $total"
    print_color $GREEN "Passed: $passed"
    print_color $RED "Failed: $failed"
    
    if [ $failed -eq 0 ]; then
        print_color $GREEN "🏆 ALL TESTS PASSED! Perfect ES6 scoping implementation! 🏆"
        return 0
    else
        print_color $YELLOW "⚠️ Some tests failed. Check the output above for details."
        return 1
    fi
}

# Main script logic
main() {
    print_color $PURPLE "🔥 UltraScript ES6 Scoping Test Suite 🔥"
    print_color $PURPLE "========================================"
    echo ""
    
    # Check dependencies first
    if ! check_dependencies; then
        exit 1
    fi
    
    # Parse command line arguments
    case "${1:-all}" in
        "list"|"ls")
            list_tests
            ;;
        "clean")
            clean_tests
            ;;
        "all")
            run_all_tests
            ;;
        "help"|"-h"|"--help")
            print_color $CYAN "Usage: $0 [command|test_name]"
            echo ""
            echo "Commands:"
            echo "  all          - Build and run all tests (default)"
            echo "  list, ls     - List available tests"
            echo "  clean        - Remove built test executables"
            echo "  help         - Show this help"
            echo ""
            echo "Test names:"
            echo "  Specify a test file name (without .cpp) to run just that test"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Run all tests"
            echo "  $0 test_raw_javascript_es6_scoping    # Run specific test"
            echo "  $0 list                               # List available tests"
            echo "  $0 clean                              # Clean executables"
            ;;
        *)
            # Try to run specific test
            local test_file="${1}.cpp"
            if [ -f "$test_file" ]; then
                build_and_run_test "$test_file"
            else
                print_color $RED "❌ Test file $test_file not found"
                echo ""
                print_color $YELLOW "Available tests:"
                list_tests
                exit 1
            fi
            ;;
    esac
}

# Run main function with all arguments
main "$@"
