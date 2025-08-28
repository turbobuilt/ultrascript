#include "compiler.h"
#include <algorithm>

void LexicalScopeNode::add_self_dependency(const std::string& var_name, int def_depth, size_t access_count) {
    // Check if we already have this dependency
    for (auto& dep : self_dependencies) {
        if (dep.variable_name == var_name && dep.definition_depth == def_depth) {
            dep.access_count += access_count;
            return;
        }
    }
    
    // Add new dependency
    ScopeDependency new_dep(var_name, def_depth);
    new_dep.access_count = access_count;
    self_dependencies.push_back(new_dep);
}

void LexicalScopeNode::add_descendant_dependency(const std::string& var_name, int def_depth, size_t access_count) {
    // Check if we already have this dependency
    for (auto& dep : descendant_dependencies) {
        if (dep.variable_name == var_name && dep.definition_depth == def_depth) {
            dep.access_count += access_count;
            return;
        }
    }
    
    // Add new dependency
    ScopeDependency new_dep(var_name, def_depth);
    new_dep.access_count = access_count;
    descendant_dependencies.push_back(new_dep);
}
