#include "compiler.h"
#include "parser_gc_integration.h"  // For ParserGCIntegration implementation
#include "x86_codegen_v2.h"  // For X86CodeGenV2 validation
#include "runtime.h"
#include "runtime_syscalls.h"
#include "goroutine_system_v2.h"
#include "function_compilation_manager.h"
#include "function_address_patching.h"
#include "ffi_syscalls.h"  // FFI integration
#include "static_analyzer.h"  // NEW static analysis pass

// Runtime function declarations
extern "C" void __register_function_code_address(const char* function_name, void* address);
// patch_all_function_addresses is C++ linkage - declared in function_address_patching.h


// External console mutex for thread safety
extern std::mutex g_console_mutex;


#include <fstream>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

// New goroutine system functions
extern "C" void __runtime_spawn_main_goroutine(void* func_ptr);
extern "C" void __runtime_wait_for_main_goroutine();

GoTSCompiler::GoTSCompiler(Backend backend) : target_backend(backend), current_parser(nullptr) {
    std::cout << "DEBUG: GoTSCompiler constructor starting" << std::endl;
    set_backend(backend);
    
    // Set up TypeInference compiler context for lexical scope integration
    type_system.set_compiler_context(this);
    
    std::cout << "DEBUG: GoTSCompiler constructor completed" << std::endl;
}

GoTSCompiler::~GoTSCompiler() {
    // Explicit destructor to ensure proper cleanup
    // The automatic destructor was causing segfaults
}

void GoTSCompiler::set_backend(Backend backend) {
    target_backend = backend;
    
    switch (backend) {
        case Backend::X86_64:
            codegen = create_x86_codegen();
            break;
    }
}

void GoTSCompiler::set_current_file(const std::string& file_path) {
    std::cout << "DEBUG: set_current_file() called with: " << file_path << std::endl;
    std::cout << "DEBUG: current_file_path before assignment: " << current_file_path << std::endl;
    current_file_path = file_path;
    std::cout << "DEBUG: current_file_path after assignment: " << current_file_path << std::endl;
}

void GoTSCompiler::compile(const std::string& source) {
    try {
        // Create error reporter with source code and file path
        ErrorReporter error_reporter(source, current_file_path);
        
        Lexer lexer(source, &error_reporter);
        auto tokens = lexer.tokenize();
        
        std::cout << "Tokens generated: " << tokens.size() << std::endl;
        
        Parser parser(std::move(tokens), &error_reporter);
        current_parser = &parser;  // Set reference for lexical scope access
        
        // PHASE 1: PARSING - Build AST with minimal scope tracking
        std::cout << "[COMPILER] PHASE 1: PARSING..." << std::endl;
        auto ast = parser.parse();
        
        std::cout << "AST nodes: " << ast.size() << std::endl;
        
        // PHASE 2: STATIC ANALYSIS - Full AST traversal for scope analysis and variable packing
        std::cout << "[COMPILER] PHASE 2: STATIC ANALYSIS..." << std::endl;
        static_analyzer_ = std::make_unique<StaticAnalyzer>();
        
        // INTEGRATION: Pass parser's scope analyzer to StaticAnalyzer for proper integration
        if (current_parser && current_parser->get_scope_analyzer()) {
            static_analyzer_->set_parser_scope_analyzer(current_parser->get_scope_analyzer());
            std::cout << "[COMPILER] Connected StaticAnalyzer to parser's scope analysis" << std::endl;
        } else {
            std::cout << "[COMPILER] WARNING: No parser scope analyzer available, using fallback analysis" << std::endl;
        }
        
        static_analyzer_->analyze(ast);
        
        // PHASE 3: CODE GENERATION - Generate code with complete static analysis
        std::cout << "[COMPILER] PHASE 3: CODE GENERATION..." << std::endl;
        
        // CREATE NEW SCOPE-AWARE CODE GENERATOR using the StaticAnalyzer
        codegen = create_scope_aware_codegen_with_static_analyzer(static_analyzer_.get());
        std::cout << "[NEW_SYSTEM] Created ScopeAwareCodeGen with complete static analysis" << std::endl;
        
        codegen->clear();
        
        // Runtime functions will be registered during runtime initialization
        // to avoid double registration and potential memory corruption
        
        // Set the compiler context for constructor code generation
        ConstructorDecl::set_compiler_context(this);
        
        // Set the compiler context for function registration
        set_current_compiler(this);
        
        // First, register all class declarations and generate default constructors if needed
        for (const auto& node : ast) {
            if (auto class_decl = dynamic_cast<ClassDecl*>(node.get())) {
                // Generate default constructor if none exists
                if (!class_decl->constructor) {
                    class_decl->constructor = std::make_unique<ConstructorDecl>(class_decl->name);
                    std::cout << "Generated default constructor for class: " << class_decl->name << std::endl;
                }
                
                // Register class in JIT system
                // Add properties to JIT class registry
                for (const auto& field : class_decl->fields) {
                    (void)field; // Suppress unused variable warning
                    uint8_t type_id = 0; // Default to ANY type
                    uint32_t size = 8;   // Default to 8 bytes (pointer size)
                    
                    // For now, treat all fields as pointers since field.type is a DataType enum
                    // TODO: Implement proper type mapping from DataType enum
                    type_id = 4; // OBJECT type
                    size = 8;    // Pointer size
                    (void)type_id; (void)size; // Suppress unused variable warnings
                }
                
                // Keep old system for compatibility
                ClassInfo class_info(class_decl->name);
                class_info.fields = class_decl->fields;
                class_info.parent_classes = class_decl->parent_classes;
                // instance_size will be calculated in register_class() to handle inheritance
                register_class(class_info);
                
                // Get the processed class info to show correct field count (including inherited)
                ClassInfo* final_class_info = get_class(class_decl->name);
                std::cout << "Registered class: " << class_decl->name << " with " << final_class_info->fields.size() << " fields";
                if (!class_decl->parent_classes.empty()) {
                    std::cout << " (extends ";
                    for (size_t i = 0; i < class_decl->parent_classes.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << class_decl->parent_classes[i];
                    }
                    std::cout << ")";
                }
                std::cout << std::endl;
                
                // CRITICAL: Register operator overloads BEFORE any code generation that might use them
                for (const auto& op_overload : class_decl->operator_overloads) {
                    // Generate parameter signature for unique function naming
                    std::string param_signature = "";
                    for (size_t i = 0; i < op_overload->parameters.size(); ++i) {
                        if (i > 0) param_signature += "_";
                        if (op_overload->parameters[i].type == DataType::ANY) {
                            param_signature += "any";
                        } else {
                            param_signature += std::to_string(static_cast<int>(op_overload->parameters[i].type));
                        }
                    }
                    
                    std::string op_function_name = class_decl->name + "::__op_" + std::to_string(static_cast<int>(op_overload->operator_type)) + "_" + param_signature + "__";
                    OperatorOverload overload(op_overload->operator_type, op_overload->parameters, op_overload->return_type);
                    overload.function_name = op_function_name;
                    register_operator_overload(class_decl->name, overload);
                    std::cout << "Pre-registered operator overload " << op_function_name 
                              << " for class " << class_decl->name << " with operator type " << static_cast<int>(op_overload->operator_type) << std::endl;
                }
            }
        }
        
        // NEW THREE-PHASE COMPILATION SYSTEM
        FunctionCompilationManager::instance().clear();
        FunctionCompilationManager::instance().discover_functions(ast);
        
        // PHASE 2: FUNCTION COMPILATION
        // Compile all functions to the beginning of the code section
        FunctionCompilationManager::instance().compile_all_functions(*codegen);
        
        // Check if we have any function declarations or class definitions
        bool has_functions = false;
        bool has_classes = false;
        for (const auto& node : ast) {
            if (dynamic_cast<FunctionDecl*>(node.get())) {
                has_functions = true;
            }
            if (dynamic_cast<ClassDecl*>(node.get())) {
                has_classes = true;
            }
        }
        
        // Only generate a jump to main if we have function declarations or classes to skip
        if (has_functions || has_classes) {
            codegen->emit_jump("__main");
        }
        
        // Generate all function declarations first
        for (const auto& node : ast) {
            if (dynamic_cast<FunctionDecl*>(node.get())) {
                node->generate_code(*codegen);
            }
        }
        
        // Generate all class constructors and methods before main code
        for (const auto& node : ast) {
            if (auto class_decl = dynamic_cast<ClassDecl*>(node.get())) {
                // Generate constructor first if it exists
                if (class_decl->constructor) {
                    class_decl->constructor->generate_code(*codegen);
                }
                
                // Generate methods
                for (auto& method : class_decl->methods) {
                    method->generate_code(*codegen);
                }
                
                // Generate operator overloads
                for (auto& op_overload : class_decl->operator_overloads) {
                    op_overload->generate_code(*codegen);
                }
            }
        }
        
        // PHASE 2.1: GENERATE SPECIALIZED INHERITED METHODS
        // After all methods are generated, create specialized versions for inherited methods
        for (const auto& node : ast) {
            if (auto class_decl = dynamic_cast<ClassDecl*>(node.get())) {
                generate_specialized_inherited_methods(*class_decl, *codegen, type_system);
            }
        }
        
        // PHASE 2.5: PREPARE FOR MAIN CODE GENERATION
        // At this point, we need to create executable memory and assign function addresses
        // before generating main code, so function expressions can use direct addresses
        
        // Generate main code label
        codegen->emit_label("__main");
        
        // Calculate stack size for main function based on statement complexity
        size_t non_function_statements = 0;
        for (const auto& node : ast) {
            if (!dynamic_cast<FunctionDecl*>(node.get())) {
                non_function_statements++;
            }
        }
        
        // Estimate stack size: base + (statements * complexity factor) + method call overhead
        int64_t estimated_stack_size = 80 + (non_function_statements * 24) + 64;
        // Ensure 16-byte alignment
        if (estimated_stack_size % 16 != 0) {
            estimated_stack_size += 16 - (estimated_stack_size % 16);
        }
        
        // Set stack size for main function
        codegen->set_function_stack_size(estimated_stack_size);
        
        // CRITICAL: Reset stack frame before main function to prevent function compilation pollution
        if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(codegen.get())) {
            x86_gen->reset_stack_frame_for_new_function();
        }
        
        codegen->emit_prologue();
        
        // NEW STATIC ANALYZER SYSTEM: Main function scope tracking via StaticAnalyzer
        std::cout << "[MAIN_SCOPE_DEBUG] Using StaticAnalyzer for main function" << std::endl;
        
        // Set the global scope as current for main function code generation
        LexicalScopeNode* global_scope = static_analyzer_->get_scope_node_for_depth(1);
        if (global_scope) {
            // CRITICAL: Pack global scope variables before main function generation
            if (global_scope->variable_offsets.empty() && !global_scope->variable_declarations.empty()) {
                std::cout << "[MAIN_SCOPE_DEBUG] Triggering deferred packing for global scope with " 
                          << global_scope->variable_declarations.size() << " variables" << std::endl;
                static_analyzer_->perform_deferred_packing_for_scope(global_scope);
            }
            
            set_current_scope(global_scope);
            std::cout << "[MAIN_SCOPE_DEBUG] Set global scope (depth 1) as current scope for main function" << std::endl;
            std::cout << "[MAIN_SCOPE_DEBUG] Global scope address: " << (void*)global_scope << std::endl;
            std::cout << "[MAIN_SCOPE_DEBUG] Global scope has " << global_scope->variable_offsets.size() << " packed variables" << std::endl;
            
            // CRITICAL: Actually allocate memory for global scope and set up r15
            std::cout << "[MAIN_SCOPE_DEBUG] Allocating memory for global scope and setting up r15" << std::endl;
            emit_scope_enter(*codegen, global_scope);
        } else {
            std::cerr << "[ERROR] No global scope found for main function code generation" << std::endl;
        }
        
        // Process imports first (they are hoisted like in JavaScript/TypeScript)
        for (const auto& node : ast) {
            if (dynamic_cast<ImportStatement*>(node.get())) {
                node->generate_code(*codegen);
            }
        }
        
        // Generate non-function, non-import statements
        for (const auto& node : ast) {
            if (!dynamic_cast<FunctionDecl*>(node.get()) && !dynamic_cast<ImportStatement*>(node.get())) {
                node->generate_code(*codegen);
            }
        }
        
        // Add explicit jump to epilogue to prevent fall-through
        codegen->emit_jump("__main_epilogue");
        
        // Mark epilogue location  
        codegen->emit_label("__main_epilogue");
        
        // CRITICAL: Exit the global scope to restore stack balance
        // TEMPORARILY DISABLED: This was causing segfaults by freeing memory before epilogue
        // if (global_scope) {
        //     std::cout << "[MAIN_SCOPE_DEBUG] Exiting global scope before epilogue to restore stack" << std::endl;
        //     emit_scope_exit(*codegen, global_scope);
        // }
        
        // CRITICAL: Add automatic reference count cleanup for local variables before epilogue
        // TEMPORARILY DISABLED: Let functions handle their own cleanup
        // generate_scope_cleanup_code(*codegen, type_system);
        
        // Ensure return value is set to 0 for main function
        codegen->emit_mov_reg_imm(0, 0);  // mov rax, 0
        
        // Generate function epilogue
        codegen->emit_epilogue();
        
        // CRITICAL: Validate code generation before proceeding
        auto* x86_codegen = dynamic_cast<X86CodeGenV2*>(codegen.get());
        if (x86_codegen && !x86_codegen->validate_code_generation()) {
            throw std::runtime_error("Code generation validation failed - aborting compilation");
        }
        
        std::cout << "Code generation completed. Machine code size: " 
                  << codegen->get_code().size() << " bytes" << std::endl;
        
        // CRITICAL: Explicitly clear AST before parser destruction to avoid cleanup issues
        std::cout << "DEBUG: Explicitly clearing AST (" << ast.size() << " nodes) before parser destruction" << std::endl;
        ast.clear();  // This destroys all AST nodes BEFORE parser goes out of scope
        std::cout << "DEBUG: AST cleared successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Compilation error: " << e.what() << std::endl;
        throw;
    }
}

// Parse-only method for testing scope analysis
std::vector<std::unique_ptr<ASTNode>> GoTSCompiler::parse_javascript(const std::string& source) {
    try {
        // Create error reporter with source code and file path
        ErrorReporter error_reporter(source, current_file_path);
        
        Lexer lexer(source, &error_reporter);
        auto tokens = lexer.tokenize();
        
        std::cout << "Tokens generated: " << tokens.size() << std::endl;
        
        Parser parser(std::move(tokens), &error_reporter);
        current_parser = &parser;  // Set reference for lexical scope access
        auto ast = parser.parse();
        
        std::cout << "AST nodes parsed: " << ast.size() << std::endl;
        
        return ast;
        
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        throw;
    }
}

void GoTSCompiler::generate_scope_cleanup_code(CodeGenerator& gen, TypeInference& types) {
    // Generate MAXIMUM PERFORMANCE cleanup for all local CLASS_INSTANCE variables
    // For compile-time known types: Generate direct destructor calls (zero overhead)
    // For runtime types: Use reference counting (minimal overhead)
    
    // Get all variables allocated in the current scope
    const auto& variable_types = types.get_all_variable_types();
    const auto& variable_offsets = types.get_all_variable_offsets();
    const auto& variable_class_names = types.get_all_variable_class_names();
    
    std::cout << "[DEBUG] generate_scope_cleanup_code: Processing " << variable_types.size() << " variables" << std::endl;
    
    for (const auto& [name, type] : variable_types) {
        // Skip 'this' variable - it's not a local variable that needs cleanup
        if (name == "this") {
            std::cout << "[DEBUG] Skipping 'this' variable - not a local variable" << std::endl;
            continue;
        }
        
        if (type == DataType::CLASS_INSTANCE) {
            // This is a class instance variable - needs reference count cleanup
            auto offset_it = variable_offsets.find(name);
            if (offset_it != variable_offsets.end()) {
                int64_t offset = offset_it->second;
                
                std::cout << "[DEBUG] Generating cleanup code for variable '" << name 
                          << "' at offset " << offset << std::endl;
                
                std::cout << "[DEBUG] CLEANUP: About to read from stack offset " << offset 
                          << " (should match assignment offset)" << std::endl;
                
                // MAXIMUM PERFORMANCE: Check if we know the class type at compile time
                auto class_name_it = variable_class_names.find(name);
                if (class_name_it != variable_class_names.end() && !class_name_it->second.empty()) {
                    // ZERO OVERHEAD PATH: Direct destructor call for known types
                    std::string class_name = class_name_it->second;
                    std::cout << "[DEBUG] DIRECT DESTRUCTOR: Generating direct call for class " << class_name << std::endl;
                    
                    // 1. Load the object pointer from the stack variable
                    gen.emit_mov_reg_mem(1, offset); // RCX = [RBP + offset] (object pointer)
                    
                    // DEBUG: Call runtime debug function to track what's being loaded
                    gen.emit_mov_reg_reg(7, 5);  // RDI = RBP (frame pointer)
                    gen.emit_mov_reg_imm(6, offset);  // RSI = offset  
                    gen.emit_mov_reg_reg(2, 1);  // RDX = value loaded (RCX)
                    gen.emit_call("__debug_stack_load");
                    
                    // 2. Check if pointer is null (don't call destructor on null pointers)
                    gen.emit_mov_reg_imm(2, 0); // RDX = 0
                    gen.emit_compare(1, 2); // Compare RCX with 0
                    std::string skip_cleanup_label = "skip_cleanup_" + name + "_" + std::to_string(rand());
                    gen.emit_jump_if_zero(skip_cleanup_label); // Skip if null
                    
                    // 3. DIRECT DESTRUCTOR CALL (zero overhead)
                    std::string destructor_label = "__method_destructor_" + class_name;
                    gen.emit_mov_reg_reg(7, 1); // RDI = RCX (move object pointer to first parameter register)
                    gen.emit_call(destructor_label); // Direct call to destructor - no lookup!
                    
                    // 4. Free the object directly (no reference counting needed for stack objects)
                    // RELOAD object pointer from stack (destructors can modify registers)
                    gen.emit_mov_reg_mem(7, offset); // RDI = [RBP + offset] (reload object pointer)
                    gen.emit_call("__object_free_direct"); // Free memory directly
                    
                    // 5. Skip cleanup label
                    gen.emit_label(skip_cleanup_label);
                    
                } else {
                    // FALLBACK PATH: Reference counting for unknown types at compile time
                    std::cout << "[DEBUG] REFERENCE COUNTING: Using ref count for unknown type" << std::endl;
                    
                    // 1. Load the object pointer from the stack variable
                    gen.emit_mov_reg_mem(1, offset); // RCX = [RBP + offset] (object pointer)
                    
                    // 2. Check if pointer is null (don't decrement null pointers)
                    gen.emit_mov_reg_imm(2, 0); // RDX = 0
                    gen.emit_compare(1, 2); // Compare RCX with 0
                    std::string skip_cleanup_label = "skip_cleanup_" + name + "_" + std::to_string(rand());
                    gen.emit_jump_if_zero(skip_cleanup_label); // Skip if null
                    
                    // 3. Decrement reference count (this may call destructor if ref_count reaches 0)
                    gen.emit_ref_count_decrement(1, 2); // Decrement ref count of object in RCX
                    
                    // 4. Skip cleanup label
                    gen.emit_label(skip_cleanup_label);
                }
            }
        } else if (type == DataType::ANY) {
            // ANY variable might contain a class instance - check at runtime
            auto offset_it = variable_offsets.find(name);
            if (offset_it != variable_offsets.end()) {
                int64_t offset = offset_it->second;
                
                std::cout << "[DEBUG] Generating runtime cleanup check for ANY variable '" << name 
                          << "' at offset " << offset << std::endl;
                
                // Generate code to check and clean up DynamicValue if it contains an object:
                // 1. Load the DynamicValue pointer from the stack variable
                gen.emit_mov_reg_mem(1, offset); // RCX = [RBP + offset] (DynamicValue*)
                
                // 2. Check if pointer is null
                gen.emit_mov_reg_imm(2, 0); // RDX = 0
                gen.emit_compare(1, 2); // Compare RCX with 0
                std::string skip_any_cleanup_label = "skip_any_cleanup_" + name + "_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_any_cleanup_label); // Skip if null
                
                // 3. Call runtime function to handle DynamicValue cleanup
                gen.emit_mov_reg_reg(7, 1); // RDI = RCX (DynamicValue*)
                gen.emit_call("__dynamic_value_release_if_object"); // Runtime handles cleanup
                
                // 4. Skip cleanup label
                gen.emit_label(skip_any_cleanup_label);
            }
        }
    }
}

std::vector<uint8_t> GoTSCompiler::get_machine_code() {
    return codegen->get_code();
}

void GoTSCompiler::execute() {
    if (target_backend == Backend::X86_64) {
        auto machine_code = get_machine_code();
        
        if (machine_code.empty()) {
            std::cerr << "No machine code to execute" << std::endl;
            return;
        }
        
        size_t code_size = machine_code.size();
        // Round up to page size for better memory management
        size_t page_size = sysconf(_SC_PAGESIZE);
        size_t aligned_size = (code_size + page_size - 1) & ~(page_size - 1);
        
        // Use MAP_PRIVATE for proper JIT memory isolation
        void* exec_mem = mmap(nullptr, aligned_size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        if (exec_mem == MAP_FAILED) {
            std::cerr << "Failed to allocate executable memory" << std::endl;
            return;
        }
        
        memcpy(exec_mem, machine_code.data(), code_size);
        
        // Store the executable memory info globally for thread access
        __set_executable_memory(exec_mem, aligned_size);
        
        __runtime_init();
        
        // Runtime registration happens automatically in new system
        
        // PRODUCTION FIX: Resolve any unresolved runtime function calls now that the registry is populated
        // We need to patch the code while it's still writable
        codegen->resolve_runtime_function_calls();

        // Apply the patches to the executable memory
        auto updated_code = codegen->get_code();
        memcpy(exec_mem, updated_code.data(), updated_code.size());

        // PRODUCTION FIX: Compile all deferred function expressions AFTER stubs are generated
        // This ensures function expressions are placed after stubs at the correct offset
        compile_deferred_function_expressions(*codegen, type_system);

        // Update the executable memory with the function expressions
        updated_code = codegen->get_code();
        memcpy(exec_mem, updated_code.data(), updated_code.size());
        
        // Make memory executable and readable, but not writable for security
        if (mprotect(exec_mem, aligned_size, PROT_READ | PROT_EXEC) != 0) {
            std::cerr << "Failed to make memory executable" << std::endl;
            munmap(exec_mem, aligned_size);
            return;
        }
        
        // PHASE 2.5: ASSIGN FUNCTION ADDRESSES
        // Now that we have executable memory, assign addresses to all functions
        FunctionCompilationManager::instance().assign_function_addresses(exec_mem, aligned_size);
        FunctionCompilationManager::instance().register_function_in_runtime();
        FunctionCompilationManager::instance().print_function_registry();
        
        // Register all functions in the runtime registry with debug output
        auto& label_offsets = codegen->get_label_offsets();
        for (const auto& label : label_offsets) {
            std::cout << "  " << label.first << " -> " << label.second << std::endl;
        }

        // First, update all FunctionDecl AST nodes with their final addresses
        for (const auto& label : label_offsets) {
            const std::string& name = label.first;
            int64_t offset = label.second;
            
            // Skip internal labels
            if (name == "__main" || name == "__main_epilogue" || 
                name.find("func_already_init_") == 0 ||
                name.find("function_call_continue_") == 0 ||
                name.find("function_type_error_") == 0) {
                continue;
            }
            
            // Calculate actual function address
            void* func_addr = reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(exec_mem) + offset
            );
            
            // TODO: Update FunctionDecl AST nodes with their final addresses
            // This requires storing the AST in the compiler class
            // For now, function addresses are handled by the function compilation manager
            
            // Also register with runtime for compatibility
            std::cout << "[EXECUTION] Registering function '" << name 
                      << "' at address " << func_addr << " (offset " << offset << ")" << std::endl;
            __register_function_code_address(name.c_str(), func_addr);
        }
        
        // PATCH ALL FUNCTION ADDRESSES: Use the new zero-cost patching system
        std::cout << "[EXECUTION] Patching all function addresses using new patching system..." << std::endl;
        
        // Temporarily make memory writable for patching
        if (mprotect(exec_mem, aligned_size, PROT_READ | PROT_WRITE) != 0) {
            std::cerr << "Failed to make memory writable for patching" << std::endl;
            munmap(exec_mem, aligned_size);
            return;
        }
        
        patch_all_function_addresses(exec_mem);
        
        // Make memory executable again after patching
        if (mprotect(exec_mem, aligned_size, PROT_READ | PROT_EXEC) != 0) {
            std::cerr << "Failed to make memory executable after patching" << std::endl;
            munmap(exec_mem, aligned_size);
            return;
        }
        
        // Find and execute main function
        auto main_it = label_offsets.find("__main");
        if (main_it == label_offsets.end()) {
            std::cerr << "Error: __main label not found" << std::endl;
            munmap(exec_mem, aligned_size);
            return;
        }
        
        
        std::cout << "DEBUG: exec_mem = " << exec_mem << std::endl;
        std::cout << "DEBUG: main offset = " << main_it->second << std::endl;
        uintptr_t calculated_addr = reinterpret_cast<uintptr_t>(exec_mem) + main_it->second;
        std::cout << "DEBUG: calculated address = " << calculated_addr << std::endl;
        std::cout << "DEBUG: calculated address hex = 0x" << std::hex << calculated_addr << std::dec << std::endl;
        
        // Try calling directly using calculated address
        typedef int(*FuncPtr)();
        FuncPtr func = reinterpret_cast<FuncPtr>(calculated_addr);
        
        // Dump first 32 bytes of generated code for debugging
        std::cout << "DEBUG: First 32 bytes of generated code: ";
        uint8_t* code_bytes = static_cast<uint8_t*>(exec_mem);
        for (int i = 0; i < 32 && i < static_cast<int>(updated_code.size()); i++) {
            printf("%02x ", code_bytes[i]);
        }
        std::cout << std::endl;
        
        // Dump the complete machine code to see the full instruction sequence
        std::cout << "DEBUG: Complete machine code (" << updated_code.size() << " bytes): ";
        for (size_t i = 0; i < updated_code.size(); i++) {
            printf("%02x ", code_bytes[i]);
            if ((i + 1) % 16 == 0) std::cout << std::endl << "  ";
        }
        std::cout << std::endl;
        
        // Also dump the full machine code size for context
        std::cout << "DEBUG: Total machine code size: " << updated_code.size() << " bytes" << std::endl;
        
        std::cout << "DEBUG: About to call function..." << std::endl;
        std::cout.flush();
        
        // Spawn the main function as the main goroutine - ALL JS runs in goroutines  
        int result = 0;
        try {
            std::cout << "DEBUG: Calling function at address 0x" << std::hex << calculated_addr << std::dec << std::endl;
            std::cout.flush();
            result = func();
            std::cout << "DEBUG: Function returned " << result << std::endl;
            {
                std::lock_guard<std::mutex> lock(g_console_mutex);
                std::cout.flush();
            }
            
            // With simplified timer system, no need to mark execution complete
            {
                std::lock_guard<std::mutex> lock(g_console_mutex);
            }
            
            // Signal main goroutine completion immediately for synchronous programs
            // This prevents hanging when no actual goroutines are spawned
            // Signal main goroutine completion for V2 system
            // EventDrivenScheduler::instance().signal_main_goroutine_completion();
            
            // Timer processing is now handled by the main goroutine's event loop
            
            // If we have timers, start the timer scheduler
            // For now, just exit cleanly since timer execution is complex
        } catch (const std::exception& e) {
            std::cerr << "Exception caught during program execution: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception caught during program execution" << std::endl;
        }
        
        // Wait for main goroutine to complete (which will wait for all its children and timers)
        // This is the ONLY wait the main loop should do - never wait for timers directly
        __runtime_wait_for_main_goroutine();
        
        __runtime_cleanup();
        
        // DON'T FREE THE EXECUTABLE MEMORY - it's needed for goroutine function calls
        // The registered functions in the function registry depend on this memory
        // This memory will be freed when the process terminates
        // munmap(exec_mem, aligned_size);
        
    } else {
        throw std::runtime_error("Unsupported backend");
    }
}

// Class management methods
void GoTSCompiler::register_class(const ClassInfo& class_info) {
    ClassInfo processed_class_info = class_info;
    
    // Handle multiple inheritance - copy parent class properties to child class as first-class properties
    if (!class_info.parent_classes.empty()) {
        std::vector<Variable> inherited_fields;
        
        // Process each parent class in order
        for (const std::string& parent_name : class_info.parent_classes) {
            ClassInfo* parent_info = get_class(parent_name);
            if (!parent_info) {
                throw std::runtime_error("Parent class '" + parent_name + "' not found for class '" + class_info.name + "'");
            }
            
            // Copy all parent fields as first-class properties
            for (const auto& parent_field : parent_info->fields) {
                // Check for field name conflicts
                bool conflict = false;
                for (const auto& existing_field : inherited_fields) {
                    if (existing_field.name == parent_field.name) {
                        std::cout << "[WARNING] Field '" << parent_field.name 
                                  << "' from parent '" << parent_name 
                                  << "' conflicts with existing field in class '" << class_info.name << "'" << std::endl;
                        conflict = true;
                        break;
                    }
                }
                if (!conflict) {
                    inherited_fields.push_back(parent_field);
                }
            }
        }
        
        // Add child-specific fields after parent fields
        for (const auto& child_field : class_info.fields) {
            // Check for conflicts with inherited fields
            bool conflict = false;
            for (const auto& inherited_field : inherited_fields) {
                if (inherited_field.name == child_field.name) {
                    std::cout << "[WARNING] Child field '" << child_field.name 
                              << "' overrides inherited field in class '" << class_info.name << "'" << std::endl;
                    conflict = true;
                    break;
                }
            }
            if (!conflict) {
                inherited_fields.push_back(child_field);
            }
        }
        
        // Update the processed class info
        processed_class_info.fields = inherited_fields;
        processed_class_info.instance_size = inherited_fields.size() * 8; // 8 bytes per property
        
        // Handle method inheritance - generate specialized methods for each inheriting class
        std::unordered_map<std::string, Function> inherited_methods = class_info.methods;
        
        // Process each parent class for method inheritance
        for (const std::string& parent_name : class_info.parent_classes) {
            ClassInfo* parent_info = get_class(parent_name);
            if (parent_info) {
                // For each inherited method, we need to generate a specialized version
                // that uses the inheriting class's property layout
                for (const auto& parent_method : parent_info->methods) {
                    if (inherited_methods.find(parent_method.first) == inherited_methods.end()) {
                        // Create specialized method for this class
                        Function specialized_method = parent_method.second;
                        
                        // Change the method name to include the inheriting class
                        specialized_method.name = class_info.name + "_" + parent_method.first;
                        
                        inherited_methods[parent_method.first] = specialized_method;
                        
                        std::cout << "[SPECIALIZED METHOD] Creating specialized method '" 
                                  << specialized_method.name << "' for class '" << class_info.name 
                                  << "' (inherited from '" << parent_name << "')" << std::endl;
                    }
                }
            }
        }
        
        processed_class_info.methods = inherited_methods;
        
        std::cout << "[MULTIPLE INHERITANCE] Class " << class_info.name << " inherits from ";
        for (size_t i = 0; i < class_info.parent_classes.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << class_info.parent_classes[i];
        }
        std::cout << ", total fields: " << inherited_fields.size() 
                  << ", total methods: " << inherited_methods.size() << std::endl;
    } else {
        // No inheritance, just set instance size for child fields
        processed_class_info.instance_size = class_info.fields.size() * 8;
    }
    
    classes[processed_class_info.name] = processed_class_info;
}

ClassInfo* GoTSCompiler::get_class(const std::string& class_name) {
    auto it = classes.find(class_name);
    return (it != classes.end()) ? &it->second : nullptr;
}

bool GoTSCompiler::is_class_defined(const std::string& class_name) {
    return classes.find(class_name) != classes.end();
}

uint32_t GoTSCompiler::get_class_type_id(const std::string& class_name) {
    // For now, use a simple hash of the class name as type ID
    // TODO: Replace with proper type ID registry from working_class_debug_test.cpp
    std::hash<std::string> hasher;
    uint32_t type_id = static_cast<uint32_t>(hasher(class_name));
    
    // Ensure type ID is non-zero (0 is reserved for unknown/invalid)
    if (type_id == 0) {
        type_id = 1;
    }
    
    return type_id;
}

std::string GoTSCompiler::get_class_name_from_type_id(uint32_t type_id) {
    // For now, we need to iterate through classes to find matching type ID
    // TODO: Replace with proper type ID registry
    for (const auto& pair : classes) {
        if (get_class_type_id(pair.first) == type_id) {
            return pair.first;
        }
    }
    return "";  // Not found
}

// Function management methods

// Module system methods
std::string GoTSCompiler::resolve_module_path(const std::string& module_path, const std::string& current_file) {
    // Handle relative and absolute paths
    std::string resolved_path = module_path;
    
    // If it's a relative path and we have a current file, resolve relative to it
    bool is_relative = (module_path.length() >= 2 && module_path.substr(0, 2) == "./") ||
                      (module_path.length() >= 3 && module_path.substr(0, 3) == "../");
    
    if (!current_file.empty() && is_relative) {
        // Extract directory from current file
        size_t last_slash = current_file.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            std::string current_dir = current_file.substr(0, last_slash + 1);
            resolved_path = current_dir + module_path;
        }
    }
    
    // Try different extensions: .gts, .ts, .js (in order of preference)
    std::vector<std::string> extensions = {".gts", ".ts", ".js"};
    
    // First try the path as-is (might already have extension)
    std::ifstream file(resolved_path);
    if (file.good()) {
        file.close();
        return resolved_path;
    }
    
    // Try with different extensions
    for (const auto& ext : extensions) {
        std::string path_with_ext = resolved_path + ext;
        std::ifstream test_file(path_with_ext);
        if (test_file.good()) {
            test_file.close();
            return path_with_ext;
        }
    }
    
    // If no file found, return original path (will cause error later)
    return resolved_path;
}

Module* GoTSCompiler::load_module(const std::string& module_path) {
    // Check if module is already loaded
    auto it = modules.find(module_path);
    if (it != modules.end() && it->second.loaded) {
        return &it->second;
    }
    
    // Resolve the actual file path using current file context
    std::string resolved_path = resolve_module_path(module_path, current_file_path);
    
    // Read the file
    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open module file: " + resolved_path);
    }
    
    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    // Parse the module
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto ast = parser.parse();
    
    // Create module entry
    Module& module = modules[module_path];
    module.path = resolved_path;
    module.ast = std::move(ast);
    module.loaded = true;
    
    // Analyze exports in the module
    bool has_named_exports = false;
    for (const auto& stmt : module.ast) {
        if (auto export_stmt = dynamic_cast<ExportStatement*>(stmt.get())) {
            if (export_stmt->is_default) {
                module.has_default_export = true;
                module.default_export_name = "default";
            } else {
                has_named_exports = true;
                // Add named exports to module
                for (const auto& spec : export_stmt->specifiers) {
                    (void)spec; // Suppress unused variable warning
                    // For now, just track that we have named exports
                    // Full implementation would analyze the actual exported values
                }
            }
        }
    }
    
    // Create synthetic default export if no default but has named exports
    if (!module.has_default_export && has_named_exports) {
        create_synthetic_default_export(module);
    }
    
    return &module;
}

void GoTSCompiler::create_synthetic_default_export(Module& module) {
    // Create a synthetic default export that is an object containing all named exports
    module.has_default_export = true;
    module.default_export_name = "__synthetic_default";
    
    // The synthetic default will be created at runtime by collecting all named exports
    // into a single object. This allows: import module from "./file" when file only
    // has named exports like: export const foo = 1; export function bar() {}
    // The result would be: module = { foo: 1, bar: [Function] }
}

// Enhanced lazy loading system implementation
Module* GoTSCompiler::load_module_lazy(const std::string& module_path) {
    // Check if module is already in cache
    auto it = modules.find(module_path);
    if (it != modules.end()) {
        Module& module = it->second;
        
        // If already loaded, return it
        if (module.is_ready()) {
            return &module;
        }
        
        // If currently loading, we have a circular import
        if (module.is_loading()) {
            handle_circular_import(module_path);
            return &module;  // Return partial module
        }
        
        // If has error, throw with stack trace
        if (module.has_error()) {
            throw std::runtime_error("Module load failed: " + module_path + "\n" + 
                                      module.load_info.error_message + "\n" + 
                                      get_import_stack_trace());
        }
    }
    
    // Check for circular import before starting load
    if (is_circular_import(module_path)) {
        std::cerr << "CIRCULAR IMPORT DETECTED: " << module_path << std::endl;
        std::cerr << get_import_stack_trace() << std::endl;
        return handle_circular_import_and_return(module_path);
    }
    
    // Start loading the module
    Module& module = modules[module_path];
    module.path = module_path;
    module.state = ModuleState::LOADING;
    module.load_info.import_stack = current_loading_stack;
    current_loading_stack.push_back(module_path);
    
    std::cerr << "LOADING MODULE: " << module_path << " (stack depth: " << current_loading_stack.size() << ")" << std::endl;
    
    try {
        // Resolve the actual file path using current file context
        std::string resolved_path = resolve_module_path(module_path, current_file_path);
        module.path = resolved_path;
        
        // Read the file
        std::ifstream file(resolved_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open module file: " + resolved_path);
        }
        
        std::string source((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Parse the module AST (but don't execute yet - that's the lazy part)
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        module.ast = parser.parse();
        
        // Analyze exports (but don't execute code yet)
        prepare_partial_exports(module);
        
        // Mark as loaded
        module.state = ModuleState::LOADED;
        module.loaded = true;  // Backward compatibility
        
        // Remove from loading stack
        current_loading_stack.pop_back();
        
        std::cerr << "MODULE LOADED SUCCESSFULLY: " << module_path << std::endl;
        
        return &module;
        
    } catch (const std::exception& e) {
        // Handle loading error
        module.state = ModuleState::ERROR;
        module.load_info.error_message = e.what();
        current_loading_stack.pop_back();
        throw;
    }
}

bool GoTSCompiler::is_circular_import(const std::string& module_path) {
    // Check if module_path is already in the loading stack
    for (const auto& loading_module : current_loading_stack) {
        if (loading_module == module_path) {
            return true;
        }
    }
    return false;
}

Module* GoTSCompiler::handle_circular_import_and_return(const std::string& module_path) {
    // Find the module in cache (it should exist since we're loading it)
    auto it = modules.find(module_path);
    if (it != modules.end()) {
        Module& module = it->second;
        
        // Mark as partial if not already
        if (module.state == ModuleState::LOADING) {
            module.state = ModuleState::PARTIAL_LOADED;
            module.exports_partial = true;
        }
        
        return &module;
    }
    
    // Create new partial module
    Module& module = modules[module_path];
    module.path = module_path;
    module.state = ModuleState::PARTIAL_LOADED;
    module.exports_partial = true;
    module.load_info.import_stack = current_loading_stack;
    
    return &module;
}

void GoTSCompiler::handle_circular_import(const std::string& module_path) {
    (void)module_path; // Suppress unused parameter warning
    // Log the circular import for debugging
    std::string stack_trace = get_import_stack_trace();
    
    // For now, just continue with partial loading
    // In production, you might want to emit a warning
    // std::cerr << "Warning: Circular import detected: " << module_path << std::endl;
    // std::cerr << "Import stack: " << stack_trace << std::endl;
}

std::string GoTSCompiler::get_import_stack_trace() const {
    std::string trace = "Import stack:\n";
    for (int i = current_loading_stack.size() - 1; i >= 0; --i) {
        trace += "  " + std::to_string(current_loading_stack.size() - i) + ". " + 
                 current_loading_stack[i] + "\n";
    }
    return trace;
}

void GoTSCompiler::execute_module_code(Module& module) {
    // Only execute if not already executed
    if (module.code_executed) {
        return;
    }
    
    // Execute the module's AST
    for (const auto& stmt : module.ast) {
        (void)stmt; // Suppress unused variable warning
        // This would execute the statements in the module
        // For now, just mark as executed
        // In real implementation, would call stmt->generate_code()
    }
    
    module.code_executed = true;
}

void GoTSCompiler::prepare_partial_exports(Module& module) {
    // Analyze exports in the module without executing code
    bool has_named_exports = false;
    
    for (const auto& stmt : module.ast) {
        if (auto export_stmt = dynamic_cast<ExportStatement*>(stmt.get())) {
            if (export_stmt->is_default) {
                module.has_default_export = true;
                module.default_export_name = "default";
            } else {
                has_named_exports = true;
                // Add named exports to module
                for (const auto& spec : export_stmt->specifiers) {
                    // Create placeholder variables for now
                    Variable placeholder;
                    placeholder.name = spec.exported_name;
                    placeholder.type = DataType::ANY;  // Will be determined later
                    module.exports[spec.exported_name] = placeholder;
                }
            }
        }
    }
    
    // Create synthetic default export if no default but has named exports
    if (!module.has_default_export && has_named_exports) {
        create_synthetic_default_export(module);
    }
}

void GoTSCompiler::compile_file(const std::string& file_path) {
    // Read and compile a file directly
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }
    
    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    compile(source);
}

// Function management methods
void GoTSCompiler::register_function(const std::string& name, const Function& func) {
    functions[name] = func;
}

Function* GoTSCompiler::get_function(const std::string& name) {
    auto it = functions.find(name);
    if (it != functions.end()) {
        return &it->second;
    }
    return nullptr;
}

bool GoTSCompiler::is_function_defined(const std::string& name) const {
    return functions.find(name) != functions.end();
}

void GoTSCompiler::register_operator_overload(const std::string& class_name, const OperatorOverload& overload) {
    auto it = classes.find(class_name);
    if (it == classes.end()) {
        throw std::runtime_error("Cannot register operator overload for undefined class: " + class_name);
    }
    
    it->second.operator_overloads[overload.operator_type].push_back(overload);
}

const std::vector<OperatorOverload>* GoTSCompiler::get_operator_overloads(const std::string& class_name, TokenType operator_type) {
    auto class_it = classes.find(class_name);
    if (class_it == classes.end()) {
        return nullptr;
    }
    
    auto op_it = class_it->second.operator_overloads.find(operator_type);
    if (op_it == class_it->second.operator_overloads.end()) {
        return nullptr;
    }
    
    return &op_it->second;
}

bool GoTSCompiler::has_operator_overload(const std::string& class_name, TokenType operator_type) {
    auto class_it = classes.find(class_name);
    if (class_it == classes.end()) {
        return false;
    }
    
    return class_it->second.operator_overloads.find(operator_type) != 
           class_it->second.operator_overloads.end();
}

bool GoTSCompiler::has_operator_overload(uint32_t class_type_id, TokenType operator_type) {
    // Convert type ID to class name and use the existing implementation
    std::string class_name = get_class_name_from_type_id(class_type_id);
    if (class_name.empty()) {
        return false;
    }
    return has_operator_overload(class_name, operator_type);
}

const OperatorOverload* GoTSCompiler::find_best_operator_overload(const std::string& class_name, TokenType operator_type, 
                                                                  const std::vector<DataType>& arg_types) {
    const auto* overloads = get_operator_overloads(class_name, operator_type);
    if (!overloads) {
        return nullptr;
    }
    
    // Find the best matching overload
    const OperatorOverload* best_match = nullptr;
    int best_score = -1;
    
    for (const auto& overload : *overloads) {
        if (overload.parameters.size() != arg_types.size()) {
            continue;
        }
        
        int score = 0;
        bool match = true;
        
        for (size_t i = 0; i < arg_types.size(); ++i) {
            DataType param_type = overload.parameters[i].type;
            DataType arg_type = arg_types[i];
            
            if (param_type == DataType::ANY) {
                // Untyped parameter matches anything
                score += 1;
            } else if (param_type == arg_type) {
                // Exact match
                score += 10;
            } else if (type_system.get_cast_type(arg_type, param_type) == param_type) {
                // Can be cast to parameter type
                score += 5;
            } else {
                match = false;
                break;
            }
        }
        
        if (match && score > best_score) {
            best_score = score;
            best_match = &overload;
        }
    }
    
    return best_match;
}

void GoTSCompiler::generate_specialized_inherited_methods(const ClassDecl& class_decl, CodeGenerator& gen, TypeInference& types) {
    // PERFORMANCE: Skip method generation for single inheritance - reuse parent methods!
    if (!needs_specialized_methods(class_decl)) {
        std::cout << "[OPTIMIZATION] Single inheritance detected for " << class_decl.name 
                  << " - reusing parent methods for maximum performance" << std::endl;
        return;
    }
    
    std::cout << "[SPECIALIZATION] Generating specialized methods for multiple inheritance class: " 
              << class_decl.name << std::endl;
    
    // For multiple inheritance, generate specialized versions with correct property offsets
    for (const std::string& parent_name : class_decl.parent_classes) {
        ClassInfo* parent_info = get_class(parent_name);
        if (!parent_info) continue;
        
        // Generate specialized version of each inherited method
        for (const auto& method_pair : parent_info->methods) {
            const std::string& method_name = method_pair.first;
            const Function& parent_method = method_pair.second;
            (void)parent_method; // Suppress unused variable warning
            
            // Skip if this class defines its own version of this method
            ClassInfo* this_class_info = get_class(class_decl.name);
            if (this_class_info && this_class_info->methods.find(method_name) != this_class_info->methods.end()) {
                continue;
            }
            
            // Generate specialized method label: __method_ChildClass_methodName
            std::string specialized_label = "__method_" + class_decl.name + "_" + method_name;
            
            std::cout << "[SPECIALIZATION] Generating " << specialized_label 
                      << " for inherited method from " << parent_name << std::endl;
            
            // Generate method prologue with child class context
            gen.emit_label(specialized_label);
            
            // Set up method prologue (same as regular methods)
            int64_t estimated_stack_size = 80; // Reasonable default
            gen.set_function_stack_size(estimated_stack_size);
            gen.emit_prologue();
            
            // Save object_address (this) from RDI
            types.set_variable_offset("__this_object_address", -8);
            gen.emit_mov_mem_reg(-8, 7); // Save object_address from RDI
            
            // Set the class context to the CHILD class (not parent) for correct property offsets
            types.set_current_class_context(class_decl.name);
            
            // TODO: Generate the method body with correct class context
            // For now, generate a simple return
            gen.emit_mov_reg_imm(0, 0); // RAX = 0
            gen.emit_epilogue();
            
            std::cout << "[SPECIALIZATION] Generated specialized method " << specialized_label << std::endl;
        }
    }
}

// PERFORMANCE OPTIMIZATION: Check if class needs specialized inherited methods
bool GoTSCompiler::needs_specialized_methods(const ClassDecl& class_decl) const {
    // Single inheritance: parent properties are placed first, so offsets are compatible
    // No need for specialized methods - use parent methods directly!
    if (class_decl.parent_classes.size() <= 1) {
        return false;
    }
    
    // Multiple inheritance: property offsets change when merging multiple parents
    // Need specialized methods with correct offsets for this class
    return true;
}

// Global compiler context for AST generation
static GoTSCompiler* current_compiler = nullptr;

void set_current_compiler(GoTSCompiler* compiler) {
    current_compiler = compiler;
}

GoTSCompiler* get_current_compiler() {
    return current_compiler;
}

// Function to compile all deferred function expressions
void compile_deferred_function_expressions(CodeGenerator& gen, TypeInference& types) {
    // Stub implementation - in a full implementation this would compile
    // any function expressions that were deferred during the initial pass
    (void)gen;
    (void)types;
}