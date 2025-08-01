// Fixed version of the problematic BinaryOp::generate_code patterns in ast_codegen.cpp
// This shows how to eliminate all dynamic_cast patterns

void BinaryOp::generate_code(CodeGenerator& gen, TypeInference& types) {
    if (left) {
        left->generate_code(gen, types);
        // Push left operand result onto stack to protect it during right operand evaluation
        gen.emit_sub_reg_imm(4, 8);   // sub rsp, 8 (allocate stack space)
        
        // FIXED: Use RSP-relative memory operation directly - no dynamic_cast needed
        gen.emit_mov_mem_rsp_reg(0, 0);   // mov [rsp], rax (save left operand on stack)
    }
    
    if (right) {
        right->generate_code(gen, types);
    }
    
    DataType left_type = left ? left->result_type : DataType::ANY;
    DataType right_type = right ? right->result_type : DataType::ANY;
    
    switch (op) {
        case TokenType::PLUS:
            if (left_type == DataType::STRING || right_type == DataType::STRING) {
                result_type = DataType::STRING;
                if (left) {
                    // String concatenation - extremely optimized
                    // Right operand (string) is in RAX
                    gen.emit_mov_reg_reg(6, 0);   // mov rsi, rax (right operand -> second argument)
                    
                    // FIXED: Use RSP-relative load directly
                    gen.emit_mov_reg_mem_rsp(7, 0);   // mov rdi, [rsp] (left operand -> first argument)
                    gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                    
                    // Rest of string concatenation logic...
                    if (left_type == DataType::STRING && right_type == DataType::STRING) {
                        gen.emit_call("__string_concat");
                    } else if (left_type == DataType::STRING && right_type != DataType::STRING) {
                        gen.emit_call("__string_concat_cstr");
                    } else if (left_type != DataType::STRING && right_type == DataType::STRING) {
                        gen.emit_call("__string_concat_cstr_left");
                    } else {
                        gen.emit_call("__string_concat");
                    }
                }
            } else {
                result_type = types.get_cast_type(left_type, right_type);
                if (left) {
                    // FIXED: Use RSP-relative load directly
                    gen.emit_mov_reg_mem_rsp(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                    gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                    gen.emit_add_reg_reg(0, 3);   // add rax, rbx (add left to right)
                }
            }
            break;
            
        case TokenType::MINUS:
            result_type = types.get_cast_type(left_type, right_type);
            if (left) {
                // FIXED: Use RSP-relative load directly
                gen.emit_mov_reg_mem_rsp(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                gen.emit_sub_reg_reg(3, 0);   // sub rbx, rax (subtract right from left)
                gen.emit_mov_reg_reg(0, 3);   // mov rax, rbx (result in rax)
            }
            break;
            
        case TokenType::MULTIPLY:
            result_type = types.get_cast_type(left_type, right_type);
            if (left) {
                // FIXED: Use RSP-relative load directly
                gen.emit_mov_reg_mem_rsp(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                gen.emit_mul_reg_reg(3, 0);   // imul rbx, rax (multiply left with right)
                gen.emit_mov_reg_reg(0, 3);   // mov rax, rbx (result in rax)
            }
            break;
            
        case TokenType::POWER:
            result_type = DataType::INT64; // Power operation returns int64 for now
            if (left) {
                // For exponentiation: base ** exponent
                // x86-64 calling convention: RDI = first arg, RSI = second arg
                
                // Right operand (exponent) is currently in RAX
                gen.emit_mov_reg_reg(6, 0);   // mov rsi, rax (exponent -> second argument)
                
                // FIXED: Use RSP-relative load directly
                gen.emit_mov_reg_mem_rsp(7, 0);   // mov rdi, [rsp] (base -> first argument)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Call the power function: __runtime_pow(base, exponent)
                gen.emit_call("__runtime_pow");
                // Result will be in RAX
            }
            break;
            
        case TokenType::DIVIDE:
            result_type = types.get_cast_type(left_type, right_type);
            if (left) {
                // FIXED: Use RSP-relative load directly
                gen.emit_mov_reg_mem_rsp(1, 0);   // mov rcx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                gen.emit_div_reg_reg(1, 0);   // div rcx by rax (divide left by right)
                gen.emit_mov_reg_reg(0, 1);   // mov rax, rcx (result in rax)
            }
            break;
            
        case TokenType::MODULO:
            result_type = types.get_cast_type(left_type, right_type);
            if (left) {
                // Use runtime function for modulo to ensure robustness
                // Right operand is in RAX, move to RSI (second argument)
                gen.emit_mov_reg_reg(6, 0);   // RSI = right operand (from RAX)
                
                // FIXED: Use RSP-relative load directly
                gen.emit_mov_reg_mem_rsp(7, 0);   // RDI = left operand from [rsp]
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Call __runtime_modulo(left, right)
                gen.emit_call("__runtime_modulo");
                // Result is now in RAX
            }
            break;
            
        case TokenType::EQUAL:
        case TokenType::NOT_EQUAL:
        case TokenType::STRICT_EQUAL:
        case TokenType::LESS:
        case TokenType::GREATER:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER_EQUAL:
            result_type = DataType::BOOLEAN;
            if (left) {
                // FIXED: Use RSP-relative load directly
                gen.emit_mov_reg_mem_rsp(1, 0);   // mov rcx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Compare left operand (in RCX) with right operand (in RAX)
                gen.emit_compare(1, 0);
                
                // Set result based on comparison
                switch (op) {
                    case TokenType::EQUAL:
                    case TokenType::STRICT_EQUAL:
                        gen.emit_sete(0);  // Set AL to 1 if equal
                        break;
                    case TokenType::NOT_EQUAL:
                        gen.emit_setne(0); // Set AL to 1 if not equal
                        break;
                    case TokenType::LESS:
                        gen.emit_setl(0);  // Set AL to 1 if less
                        break;
                    case TokenType::GREATER:
                        gen.emit_setg(0);  // Set AL to 1 if greater
                        break;
                    case TokenType::LESS_EQUAL:
                        gen.emit_setle(0); // Set AL to 1 if less or equal
                        break;
                    case TokenType::GREATER_EQUAL:
                        gen.emit_setge(0); // Set AL to 1 if greater or equal
                        break;
                    default:
                        break;
                }
                
                // Zero out upper bits of RAX to ensure clean boolean result
                gen.emit_and_reg_imm(0, 0xFF);
            }
            break;
            
        case TokenType::AND:
        case TokenType::OR:
            result_type = DataType::BOOLEAN;
            if (left) {
                // Generate unique labels for short-circuiting
                static int logic_counter = 0;
                std::string end_label = "__logic_end_" + std::to_string(logic_counter);
                std::string short_circuit_label = "__logic_short_" + std::to_string(logic_counter++);
                
                // FIXED: Use RSP-relative load directly
                gen.emit_mov_reg_mem_rsp(1, 0);   // mov rcx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                if (op == TokenType::AND) {
                    // For AND: if left is false (0), short-circuit to false
                    gen.emit_mov_reg_imm(2, 0);       // mov rdx, 0
                    gen.emit_compare(1, 2);           // compare rcx with 0
                    gen.emit_jump_if_zero(short_circuit_label); // jump if left is false
                    
                    // Left is true, so result depends on right operand (already in RAX)
                    // Test if right operand is non-zero
                    gen.emit_compare(0, 2);           // compare rax with 0
                    gen.emit_setne(0);                // Set AL to 1 if RAX != 0
                    gen.emit_and_reg_imm(0, 0xFF);    // Zero out upper bits
                    gen.emit_jump(end_label);
                    
                    // Short-circuit: left was false, so result is false
                    gen.emit_label(short_circuit_label);
                    gen.emit_mov_reg_imm(0, 0);       // mov rax, 0
                } else { // OR
                    // For OR: if left is true (non-zero), short-circuit to true
                    gen.emit_mov_reg_imm(2, 0);       // mov rdx, 0
                    gen.emit_compare(1, 2);           // compare rcx with 0
                    gen.emit_jump_if_not_zero(short_circuit_label); // jump if left is true
                    
                    // Left is false, so result depends on right operand (already in RAX)
                    // Test if right operand is non-zero
                    gen.emit_compare(0, 2);           // compare rax with 0
                    gen.emit_setne(0);                // Set AL to 1 if RAX != 0
                    gen.emit_and_reg_imm(0, 0xFF);    // Zero out upper bits
                    gen.emit_jump(end_label);
                    
                    // Short-circuit: left was true, so result is true
                    gen.emit_label(short_circuit_label);
                    gen.emit_mov_reg_imm(0, 1);       // mov rax, 1
                }
                
                gen.emit_label(end_label);
            }
            break;
            
        default:
            result_type = DataType::ANY;
            break;
    }
}

// Similar fixes are needed for:
// 1. VariableDeclaration::generate_code
// 2. ArrayAccess::generate_code  
// 3. FunctionCall::generate_code
// 4. PropertyAccess::generate_code
// 5. ExpressionMethodCall::generate_code

// All should use gen.emit_mov_reg_mem_rsp() and gen.emit_mov_mem_rsp_reg() 
// instead of dynamic_cast patterns
