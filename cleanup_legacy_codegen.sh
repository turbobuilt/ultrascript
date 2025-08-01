#!/bin/bash
# Migration script to clean up legacy dynamic_cast patterns in ast_codegen.cpp
# This script identifies and helps fix the problematic dynamic_cast patterns

set -e

echo "=== UltraScript X86 CodeGen Legacy Pattern Cleanup ==="
echo "Analyzing ast_codegen.cpp for dynamic_cast patterns..."

# Backup the original file
cp ast_codegen.cpp ast_codegen.cpp.backup
echo "Backup created: ast_codegen.cpp.backup"

# Count the problematic patterns
PATTERN_COUNT=$(grep -c "dynamic_cast.*X86CodeGen" ast_codegen.cpp || true)
echo "Found $PATTERN_COUNT dynamic_cast patterns to fix"

# Show the patterns
echo ""
echo "=== Problematic Patterns Found ==="
grep -n "dynamic_cast.*X86CodeGen" ast_codegen.cpp || echo "No patterns found"

echo ""
echo "=== Analysis ==="
echo "These patterns indicate:"
echo "1. Runtime type checking overhead"
echo "2. Violation of abstraction principles"
echo "3. Maintenance burden due to backend-specific code"
echo "4. Potential for runtime errors"

echo ""
echo "=== Recommended Fix ==="
echo "Replace all these patterns with proper virtual method calls."
echo "The CodeGenerator interface should be enhanced to support all needed operations."

# Create a sed script to show what would be replaced
cat > fix_patterns.sed << 'EOF'
# Fix pattern 1: Remove dynamic_cast for emit_mov_mem_reg
s/if (auto x86_gen = dynamic_cast<X86CodeGen\*>(&gen)) {[[:space:]]*x86_gen->emit_mov_mem_reg(\([^)]*\));[[:space:]]*} else {[[:space:]]*gen\.emit_mov_mem_reg(\1);[[:space:]]*}/gen.emit_mov_mem_reg(\1);/g

# Fix pattern 2: Remove dynamic_cast for emit_mov_reg_mem  
s/if (auto x86_gen = dynamic_cast<X86CodeGen\*>(&gen)) {[[:space:]]*x86_gen->emit_mov_reg_mem(\([^)]*\));[[:space:]]*} else {[[:space:]]*gen\.emit_mov_reg_mem(\1);[[:space:]]*}/gen.emit_mov_reg_mem(\1);/g

# Fix pattern 3: Remove static_cast assumptions
s/auto\* x86_gen = static_cast<X86CodeGen\*>(&gen);[[:space:]]*x86_gen->emit_mov_reg_mem(\([^)]*\));/gen.emit_mov_reg_mem(\1);/g
EOF

echo ""
echo "=== Preview of Automatic Fixes ==="
echo "The following changes would be made:"
sed -f fix_patterns.sed ast_codegen.cpp | diff ast_codegen.cpp - || true

echo ""
echo "=== Manual Review Required ==="
echo "Some patterns are too complex for automatic fixing and need manual review:"

# Find complex patterns that need manual attention
echo ""
echo "Complex patterns requiring manual attention:"
grep -A 5 -B 5 "dynamic_cast.*X86CodeGen" ast_codegen.cpp | head -50

echo ""
echo "=== Recommendations ==="
echo "1. Use the new X86CodeGenImproved class instead of legacy versions"
echo "2. All memory operations should go through the base CodeGenerator interface"
echo "3. Add any missing methods to the CodeGenerator interface"
echo "4. Remove all dynamic_cast and static_cast patterns"
echo "5. Test thoroughly after migration"

echo ""
echo "=== Next Steps ==="
echo "1. Review the improved X86 codegen implementation"
echo "2. Update ast_codegen.cpp to use only virtual method calls"
echo "3. Update the factory to create X86CodeGenImproved instances"
echo "4. Run comprehensive tests"

# Clean up
rm -f fix_patterns.sed

echo ""
echo "Migration analysis complete!"
