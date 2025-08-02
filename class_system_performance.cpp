#include "class_system_performance.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdlib>

namespace ultraScript {

// ClassMetadata implementation
uint16_t ClassMetadata::add_property(const std::string& prop_name, PropertyType prop_type, PropertyFlags flags) {
    DEBUG_CLASS_META("Adding property '" << prop_name << "' to class '" << class_name << "' (type: " << static_cast<int>(prop_type) << ")");
    
    // Check if property already exists
    auto it = property_name_to_index.find(prop_name);
    if (it != property_name_to_index.end()) {
        DEBUG_CLASS_META("Property '" << prop_name << "' already exists with index " << it->second);
        return it->second;  // Return existing index
    }
    
    PropertyDescriptor prop(prop_name, prop_type, flags);
    prop.index = static_cast<uint16_t>(properties.size());
    
    properties.push_back(prop);
    property_hash_to_index[prop.name_hash] = prop.index;
    property_name_to_index[prop_name] = prop.index;
    
    DEBUG_CLASS_META("Property '" << prop_name << "' added with index " << prop.index << " and hash 0x" << std::hex << prop.name_hash << std::dec);
    
    return prop.index;
}

const PropertyDescriptor* ClassMetadata::find_property(const std::string& name) const {
    DEBUG_CLASS_META("Finding property '" << name << "' in class '" << class_name << "'");
    
    auto it = property_name_to_index.find(name);
    if (it != property_name_to_index.end()) {
        DEBUG_CLASS_META("Found property '" << name << "' at index " << it->second);
        return &properties[it->second];
    }
    
    // Check inheritance chain
    DEBUG_CLASS_META("Property '" << name << "' not found in class, checking " << inheritance_chain.size() << " ancestors");
    for (ClassMetadata* ancestor : inheritance_chain) {
        DEBUG_CLASS_META("Checking ancestor '" << ancestor->class_name << "'");
        const PropertyDescriptor* prop = ancestor->find_property(name);
        if (prop) {
            DEBUG_CLASS_META("Found property '" << name << "' in ancestor '" << ancestor->class_name << "'");
            return prop;
        }
    }
    
    DEBUG_CLASS_META("Property '" << name << "' not found in class hierarchy");
    return nullptr;
}

const PropertyDescriptor* ClassMetadata::find_property_by_hash(uint32_t hash) const {
    DEBUG_CLASS_META("Finding property by hash 0x" << std::hex << hash << std::dec << " in class '" << class_name << "'");
    
    auto it = property_hash_to_index.find(hash);
    if (it != property_hash_to_index.end()) {
        DEBUG_CLASS_META("Found property by hash at index " << it->second << " ('" << properties[it->second].name << "')");
        return &properties[it->second];
    }
    
    // Check inheritance chain
    DEBUG_CLASS_META("Property not found by hash in class, checking ancestors");
    for (ClassMetadata* ancestor : inheritance_chain) {
        DEBUG_CLASS_META("Checking ancestor '" << ancestor->class_name << "' for hash");
        const PropertyDescriptor* prop = ancestor->find_property_by_hash(hash);
        if (prop) {
            DEBUG_CLASS_META("Found property by hash in ancestor '" << ancestor->class_name << "'");
            return prop;
        }
    }
    
    DEBUG_CLASS_META("Property not found by hash in class hierarchy");
    return nullptr;
}

int16_t ClassMetadata::get_property_index(const std::string& name) const {
    auto it = property_name_to_index.find(name);
    if (it != property_name_to_index.end()) {
        return static_cast<int16_t>(it->second);
    }
    
    // Check inheritance chain
    for (ClassMetadata* ancestor : inheritance_chain) {
        int16_t index = ancestor->get_property_index(name);
        if (index >= 0) return index;
    }
    
    return -1;  // Not found
}

void ClassMetadata::set_parent_class(const std::string& parent_name) {
    parent_class_name = parent_name;
    parent_metadata = ClassRegistry::instance().get_class_metadata(parent_name);
    build_inheritance_chain();
}

void ClassMetadata::finalize_layout() {
    DEBUG_CLASS_META("Finalizing layout for class '" << class_name << "' with " << properties.size() << " properties");
    calculate_property_offsets();
    build_inheritance_chain();
    DEBUG_CLASS_META("Class '" << class_name << "' layout finalized - instance_size: " << instance_size << ", data_size: " << data_size);
}

bool ClassMetadata::inherits_from(const std::string& ancestor_class_name) const {
    if (parent_class_name == ancestor_class_name) {
        return true;
    }
    
    for (ClassMetadata* ancestor : inheritance_chain) {
        if (ancestor->class_name == ancestor_class_name) {
            return true;
        }
    }
    
    return false;
}

void ClassMetadata::calculate_property_offsets() {
    DEBUG_CLASS_META("Calculating property offsets for class '" << class_name << "'");
    
    uint32_t current_offset = 0;
    
    // Start after parent class properties
    if (parent_metadata) {
        current_offset = parent_metadata->data_size;
        DEBUG_CLASS_META("Starting offset at " << current_offset << " (parent class '" << parent_metadata->class_name << "' size)");
    }
    
    // Calculate offsets for this class's properties
    for (PropertyDescriptor& prop : properties) {
        // Align to property type requirements
        size_t alignment = get_property_type_alignment(prop.type);
        current_offset = (current_offset + alignment - 1) & ~(alignment - 1);
        
        prop.offset = current_offset;
        current_offset += static_cast<uint32_t>(get_property_type_size(prop.type));
        
        DEBUG_CLASS_META("Property '" << prop.name << "' offset: " << prop.offset << ", size: " << get_property_type_size(prop.type));
    }
    
    data_size = current_offset;
    instance_size = sizeof(ObjectHeader) + data_size + sizeof(void*); // Extra space for dynamic properties map
    
    DEBUG_CLASS_META("Total data_size: " << data_size << ", instance_size: " << instance_size);
}

void ClassMetadata::build_inheritance_chain() {
    inheritance_chain.clear();
    
    ClassMetadata* current = parent_metadata;
    while (current) {
        DEBUG_CLASS_META("Adding '" << current->class_name << "' to inheritance chain for '" << class_name << "'");
        inheritance_chain.push_back(current);
        current = current->parent_metadata;
    }
    
    DEBUG_CLASS_META("Built inheritance chain for '" << class_name << "' with " << inheritance_chain.size() << " ancestors");
}

// ==================== ClassRegistry Implementation ====================

ObjectTypeId ClassRegistry::register_class(const std::string& class_name) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    DEBUG_CLASS_META("Registering class '" << class_name << "'");
    
    auto it = class_name_to_metadata.find(class_name);
    if (it != class_name_to_metadata.end()) {
        DEBUG_CLASS_META("Class '" << class_name << "' already registered with type ID " << static_cast<uint32_t>(it->second->type_id));
        return it->second->type_id;
    }
    
    ObjectTypeId type_id = static_cast<ObjectTypeId>(next_type_id.fetch_add(1));
    auto metadata = std::make_unique<ClassMetadata>(class_name, type_id);
    
    ClassMetadata* meta_ptr = metadata.get();
    class_name_to_metadata[class_name] = std::move(metadata);
    type_id_to_metadata[type_id] = meta_ptr;
    
    std::cout << "[ClassRegistry] Registered class '" << class_name 
              << "' with type_id=" << static_cast<uint32_t>(type_id) << std::endl;
    
    return type_id;
}

ClassMetadata* ClassRegistry::get_class_metadata(const std::string& class_name) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    auto it = class_name_to_metadata.find(class_name);
    return (it != class_name_to_metadata.end()) ? it->second.get() : nullptr;
}

ClassMetadata* ClassRegistry::get_class_metadata(ObjectTypeId type_id) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    auto it = type_id_to_metadata.find(type_id);
    return (it != type_id_to_metadata.end()) ? it->second : nullptr;
}

bool ClassRegistry::class_exists(const std::string& class_name) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    return class_name_to_metadata.find(class_name) != class_name_to_metadata.end();
}

void ClassRegistry::set_inheritance(const std::string& child_class, const std::string& parent_class) {
    ClassMetadata* child_meta = get_class_metadata(child_class);
    ClassMetadata* parent_meta = get_class_metadata(parent_class);
    
    if (child_meta && parent_meta) {
        child_meta->set_parent_class(parent_class);
        std::cout << "[ClassRegistry] Set inheritance: " << child_class 
                  << " extends " << parent_class << std::endl;
    }
}

void ClassRegistry::finalize_all_classes() {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    // Finalize classes in dependency order (parents first)
    std::vector<ClassMetadata*> to_finalize;
    for (auto& pair : class_name_to_metadata) {
        to_finalize.push_back(pair.second.get());
    }
    
    // Sort by inheritance depth (parents first)
    std::sort(to_finalize.begin(), to_finalize.end(), 
              [](ClassMetadata* a, ClassMetadata* b) {
                  return a->inheritance_chain.size() < b->inheritance_chain.size();
              });
    
    for (ClassMetadata* meta : to_finalize) {
        meta->finalize_layout();
        std::cout << "[ClassRegistry] Finalized class '" << meta->class_name 
                  << "' - instance_size=" << meta->instance_size 
                  << ", data_size=" << meta->data_size 
                  << ", properties=" << meta->properties.size() << std::endl;
    }
}

// ==================== ObjectInstance Implementation ====================

void* ObjectInstance::get_dynamic_property_by_hash(uint32_t name_hash) {
    DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Checking dynamic properties for hash 0x" << std::hex << name_hash << std::dec);
    
    if (!dynamic_properties) {
        DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: No dynamic properties map - returning undefined");
        return nullptr;  // No dynamic properties set yet
    }
    
    auto it = dynamic_properties->find(name_hash);
    if (it != dynamic_properties->end()) {
        DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Found dynamic property");
        return &it->second;  // Return pointer to DynamicValue
    }
    
    DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Dynamic property not found - returning undefined");
    return nullptr;
}

bool ObjectInstance::set_dynamic_property_by_hash(uint32_t name_hash, const void* value, size_t value_size) {
    DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Setting dynamic property hash 0x" << std::hex << name_hash << std::dec);
    
    if (!dynamic_properties) {
        DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Creating dynamic properties map (lazy initialization)");
        // Lazy initialization of dynamic properties map
        dynamic_properties = new std::unordered_map<uint32_t, DynamicValue>();
    }
    
    // Create DynamicValue from raw data
    DynamicValue dyn_value;
    if (value_size == sizeof(int64_t)) {
        dyn_value = DynamicValue(*static_cast<const int64_t*>(value));
        DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Stored as int64_t value");
    } else if (value_size == sizeof(double)) {
        dyn_value = DynamicValue(*static_cast<const double*>(value));
        DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Stored as double value");
    } else if (value_size == sizeof(char*)) {
        dyn_value = DynamicValue(*static_cast<const char* const*>(value));
        DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Stored as string value");
    } else {
        // For other types, store as raw bytes in DynamicValue
        // This is a simplified implementation - full implementation would handle all types
        dyn_value = DynamicValue();  // Default to empty value
        DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Stored as default value (unsupported type, size=" << value_size << ")");
    }
    
    (*dynamic_properties)[name_hash] = dyn_value;
    DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Successfully added dynamic property");
    return true;
}

std::unordered_map<uint32_t, DynamicValue>* ObjectInstance::get_dynamic_properties_map() {
    if (!dynamic_properties) {
        DEBUG_PROPERTY_ACCESS("DYNAMIC_DICT: Creating dynamic properties map");
        dynamic_properties = new std::unordered_map<uint32_t, DynamicValue>();
    }
    return dynamic_properties;
}

// ==================== ObjectFactory Implementation ====================

ObjectInstance* ObjectFactory::create_object(const std::string& class_name) {
    DEBUG_CLASS_META("Creating object of class '" << class_name << "'");
    
    ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(class_name);
    if (!meta) {
        DEBUG_CLASS_META("Failed to create object - class '" << class_name << "' not found");
        std::cerr << "[ObjectFactory] Error: Class '" << class_name << "' not found" << std::endl;
        return nullptr;
    }
    
    return create_object(meta->type_id);
}

ObjectInstance* ObjectFactory::create_object(ObjectTypeId type_id) {
    DEBUG_CLASS_META("Creating object with type ID " << static_cast<uint32_t>(type_id));
    
    ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(type_id);
    if (!meta) {
        DEBUG_CLASS_META("Failed to create object - metadata not found for type ID");
        std::cerr << "[ObjectFactory] Error: Type ID " << static_cast<uint32_t>(type_id) << " not found" << std::endl;
        return nullptr;
    }
    
    ObjectInstance* obj = allocate_object(meta);
    if (obj) {
        initialize_object(obj, meta);
        DEBUG_CLASS_META("Successfully created object of class '" << meta->class_name << "'");
    }
    
    return obj;
}

ObjectInstance* ObjectFactory::create_object_with_args(const std::string& class_name, 
                                                     const std::vector<DynamicValue>& args) {
    ObjectInstance* obj = create_object(class_name);
    if (!obj) return nullptr;
    
    // TODO: Call constructor with arguments
    // For now, just create the object
    
    return obj;
}

void ObjectFactory::destroy_object(ObjectInstance* obj) {
    if (!obj) return;
    
    // Clean up dynamic properties if they exist
    if (obj->dynamic_properties) {
        delete obj->dynamic_properties;
    }
    
    // TODO: Call destructor if defined
    
    free(obj);
}

ObjectInstance* ObjectFactory::allocate_object(ClassMetadata* meta) {
    size_t total_size = meta->instance_size;
    DEBUG_CLASS_META("Allocating " << total_size << " bytes for object of class '" << meta->class_name << "'");
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(std::aligned_alloc(64, total_size));  // 64-byte aligned
    if (!obj) {
        DEBUG_CLASS_META("Failed to allocate memory");
        std::cerr << "[ObjectFactory] Error: Failed to allocate " << total_size << " bytes for object" << std::endl;
        return nullptr;
    }
    
    DEBUG_CLASS_META("Successfully allocated memory at " << obj);
    return obj;
}

void ObjectFactory::initialize_object(ObjectInstance* obj, ClassMetadata* meta) {
    DEBUG_CLASS_META("Initializing object of class '" << meta->class_name << "' at " << obj);
    
    // Initialize header
    obj->header.type_id = meta->type_id;
    obj->header.ref_count = 1;
    obj->header.property_count = static_cast<uint16_t>(meta->properties.size());
    obj->header.flags = 0;
    
    // Zero-initialize all property data
    std::memset(obj->data, 0, meta->data_size);
    
    // Initialize dynamic properties pointer to null
    obj->dynamic_properties = nullptr;
    
    DEBUG_CLASS_META("Object initialized with " << obj->header.property_count << " properties");
    std::cout << "[ObjectFactory] Created object of class '" << meta->class_name 
              << "' with " << meta->properties.size() << " properties" << std::endl;
}

// ==================== ClassCodeGenerator Implementation ====================

void ClassCodeGenerator::generate_property_access_by_index(CodeGenerator& gen, uint16_t property_index, PropertyType prop_type) {
    DEBUG_CODEGEN("Generating ULTRA-FAST property access by index " << property_index << " (type: " << static_cast<int>(prop_type) << ")");
    
    // Emit debug code in generated assembly
    emit_debug_property_access(gen, "ULTRA-FAST", "index " + std::to_string(property_index));
    
    // Generate ultra-fast property access code
    // Input: RAX = object pointer, property_index = compile-time constant
    
    // Get ClassMetadata pointer (compile-time known)
    // mov rdx, [rax]        ; Load object header
    // mov edx, [rdx]        ; Load type_id from header
    // mov rdx, type_id_to_metadata[rdx]  ; Get metadata pointer (compile-time lookup)
    
    // For now, emit call to runtime function with compile-time optimizations
    gen.emit_mov_reg_imm(1, property_index);  // RDX = property_index
    gen.emit_call("__object_get_property_by_index_fast");
    
    DEBUG_CODEGEN("Generated direct offset assembly for property index " << property_index);
    
    // TODO: Generate direct memory access code:
    // mov rdx, [metadata + offsetof(ClassMetadata, properties)]
    // mov edx, [rdx + property_index * sizeof(PropertyDescriptor) + offsetof(PropertyDescriptor, offset)]
    // add rax, rdx          ; RAX = object + property_offset
}

void ClassCodeGenerator::generate_property_access_by_hash(CodeGenerator& gen, uint32_t name_hash) {
    DEBUG_CODEGEN("Generating DYNAMIC property access by hash 0x" << std::hex << name_hash << std::dec);
    
    // Emit debug code in generated assembly
    emit_debug_property_access(gen, "DYNAMIC", "hash 0x" + std::to_string(name_hash));
    
    // Generate dynamic property access code
    gen.emit_mov_reg_imm(1, name_hash);  // RDX = name_hash
    gen.emit_call("__object_get_property_by_hash_fast");
    
    DEBUG_CODEGEN("Generated hash lookup assembly for property hash");
    
    // Generated assembly would be something like:
    /*
    ; Debug output
    call emit_debug_cout  ; "DYNAMIC property access via hash 0xXXXX"
    
    ; Hash-based lookup
    call get_property_by_hash
    test rax, rax
    jz check_dynamic_properties
    ; ... continue with found property
    */
}

void ClassCodeGenerator::generate_property_assignment_by_index(CodeGenerator& gen, uint16_t property_index, PropertyType prop_type) {
    DEBUG_CODEGEN("Generating ULTRA-FAST property assignment by index " << property_index << " (type: " << static_cast<int>(prop_type) << ")");
    
    emit_debug_property_set(gen, "ULTRA-FAST", "index " + std::to_string(property_index));
    
    // Generate ultra-fast property assignment code
    // Input: RAX = object pointer, RDX = value, property_index = compile-time constant
    
    gen.emit_mov_reg_imm(2, property_index);  // R8 = property_index
    
    switch (prop_type) {
        case PropertyType::INT64:
        case PropertyType::UINT64:
            gen.emit_call("__object_set_property_by_index_int64");
            DEBUG_CODEGEN("Generated int64 assignment");
            break;
        case PropertyType::FLOAT64:
            gen.emit_call("__object_set_property_by_index_double");
            DEBUG_CODEGEN("Generated double assignment");
            break;
        case PropertyType::OBJECT_PTR:
        case PropertyType::STRING:
            gen.emit_call("__object_set_property_by_index_ptr");
            DEBUG_CODEGEN("Generated pointer assignment");
            break;
        default:
            gen.emit_call("__object_set_property_by_index_dynamic");
            DEBUG_CODEGEN("Generated dynamic assignment");
            break;
    }
    
    DEBUG_CODEGEN("Generated direct assignment assembly for property index " << property_index);
}

void ClassCodeGenerator::generate_object_construction(CodeGenerator& gen, ObjectTypeId type_id) {
    DEBUG_CODEGEN("Generating object construction for type ID " << static_cast<uint32_t>(type_id));
    
    // Generate object construction code
    gen.emit_mov_reg_imm(0, static_cast<uint32_t>(type_id));  // RDI = type_id
    gen.emit_call("__object_create_by_type_id_fast");
    // Result in RAX
    
    DEBUG_CODEGEN("Generated object construction assembly");
}

void ClassCodeGenerator::generate_method_call(CodeGenerator& gen, const std::string& class_name, const std::string& method_name) {
    DEBUG_CODEGEN("Generating method call for " << class_name << "::" << method_name);
    
    // Generate method call code
    // TODO: Implement method call generation
    // For now, use runtime lookup
    static std::unordered_map<std::string, const char*> string_pool;
    
    auto get_pooled_string = [&](const std::string& str) -> const char* {
        auto it = string_pool.find(str);
        if (it == string_pool.end()) {
            char* str_copy = new char[str.length() + 1];
            strcpy(str_copy, str.c_str());
            string_pool[str] = str_copy;
            return str_copy;
        }
        return it->second;
    };
    
    const char* method_name_ptr = get_pooled_string(method_name);
    gen.emit_mov_reg_imm(1, reinterpret_cast<int64_t>(method_name_ptr));  // RDX = method_name
    gen.emit_call("__object_call_method");
    
    DEBUG_CODEGEN("Generated method call assembly for " << method_name);
}

void ClassCodeGenerator::generate_instanceof_check(CodeGenerator& gen, const std::string& class_name) {
    DEBUG_CODEGEN("Generating instanceof check for class '" << class_name << "'");
    
    // Generate instanceof check code
    // TODO: Implement instanceof check generation
    gen.emit_call("__object_instanceof");
    // Result in RAX (boolean)
    
    DEBUG_CODEGEN("Generated instanceof check assembly");
}

// Debug helper methods for generated code
void ClassCodeGenerator::emit_debug_property_access(CodeGenerator& gen, const std::string& path_type, const std::string& info) {
#if ULTRASCRIPT_DEBUG_CODE_GENERATION
    // This would emit actual assembly code that calls a debug function
    // For now, just emit a comment in the generated code
    DEBUG_CODEGEN("Emitting debug code: [" << path_type << " ACCESS] " << info);
    
    /*
    Generated assembly debug code:
    
    push rdi
    push rsi
    lea rdi, [debug_msg_property_access]  ; "ULTRA-FAST ACCESS: index X"
    call puts
    pop rsi
    pop rdi
    */
    
    // Emit inline debug call in generated assembly
    gen.emit_debug_output(std::string("[") + path_type + " ACCESS] " + info);
#endif
}

void ClassCodeGenerator::emit_debug_property_set(CodeGenerator& gen, const std::string& path_type, const std::string& info) {
#if ULTRASCRIPT_DEBUG_CODE_GENERATION
    DEBUG_CODEGEN("Emitting debug code: [" << path_type << " SET] " << info);
    
    /*
    Generated assembly debug code for property setting
    */
    
    // Emit inline debug call in generated assembly  
    gen.emit_debug_output(std::string("[") + path_type + " SET] " + info);
#endif
}

} // namespace ultraScript
