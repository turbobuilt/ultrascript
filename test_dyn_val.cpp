extern "C" { void* __dynamic_value_create_from_double(int64_t); } int main() { void* result = __dynamic_value_create_from_double(0x4045000000000000LL); printf("Result: %p\\n", result); return 0; }
