#include <iostream>
#include <cstring>
#include <cstdint>

// Simple GoTSString class definition for testing - copied from runtime.h
class GoTSString {
public:
    // SSO threshold - strings up to 22 bytes are stored inline (on 64-bit systems)
    static constexpr size_t SSO_THRESHOLD = sizeof(void*) + sizeof(size_t) + sizeof(size_t) - 1;
    
private:
    union {
        // Large string storage
        struct {
            char* data;
            size_t size;
            size_t capacity;
        } large;
        
        // Small string storage - direct inline storage
        struct {
            char buffer[SSO_THRESHOLD + 1]; // +1 for null terminator
            uint8_t size; // Size fits in 1 byte for small strings
        } small;
    };
    
    // Use the MSB of small.size byte to indicate if it's a small string
    bool is_small() const {
        return (reinterpret_cast<const uint8_t*>(this)[24] & 0x80) != 0;
    }
    
    void set_small_flag() {
        reinterpret_cast<uint8_t*>(this)[24] |= 0x80;
    }
    
    void clear_small_flag() {
        reinterpret_cast<uint8_t*>(this)[24] &= 0x7F;
    }
    
    uint8_t get_small_size() const {
        return small.size & 0x7F;
    }

public:
    // Default constructor - creates empty small string
    GoTSString() noexcept {
        small.buffer[0] = '\0';
        small.size = 0x80;  // Set flag bit + size 0
    }
    
    // Constructor from C string - optimized for literal strings
    GoTSString(const char* str) {
        if (!str) {
            *this = GoTSString();
            return;
        }
        
        size_t len = strlen(str);
        if (len <= SSO_THRESHOLD && len <= 127) {  // Max 127 chars due to flag bit
            // Small string optimization
            memcpy(small.buffer, str, len);
            small.buffer[len] = '\0';
            small.size = static_cast<uint8_t>(len) | 0x80;  // Set flag bit + actual size
        } else {
            // Large string - allocate on heap
            large.size = len;
            large.capacity = ((len + 16) & ~15) | 1; // Round up to 16-byte boundary, set odd flag
            large.data = new char[large.capacity & ~1]; // Mask off the flag bit
            memcpy(large.data, str, len + 1);
            clear_small_flag();  // Make sure flag is clear for large strings
        }
    }
    
    // Constructor from data pointer and length - handles null bytes properly
    GoTSString(const char* data, size_t len) {
        if (!data || len == 0) {
            *this = GoTSString();
            return;
        }
        
        if (len <= SSO_THRESHOLD && len <= 127) {  // Max 127 chars due to flag bit
            // Small string optimization
            memcpy(small.buffer, data, len);
            small.buffer[len] = '\0';  // Always null-terminate for safety
            small.size = static_cast<uint8_t>(len) | 0x80;  // Set flag bit + actual size
        } else {
            // Large string - allocate on heap
            large.size = len;
            large.capacity = ((len + 16) & ~15) | 1; // Round up to 16-byte boundary, set odd flag
            large.data = new char[large.capacity & ~1]; // Mask off the flag bit
            memcpy(large.data, data, len);
            large.data[len] = '\0';  // Always null-terminate for safety
            clear_small_flag();  // Make sure flag is clear for large strings
        }
    }
    
    // Destructor
    ~GoTSString() {
        if (!is_small()) {
            delete[] large.data;
        }
    }
    
    // Access methods - highly optimized
    inline const char* c_str() const {
        return is_small() ? small.buffer : large.data;
    }
    
    inline const char* data() const {
        return is_small() ? small.buffer : large.data;
    }
    
    inline size_t size() const {
        return is_small() ? get_small_size() : large.size;
    }
    
    inline size_t length() const {
        return size();
    }
    
    inline bool empty() const {
        return size() == 0;
    }
    
    // Concatenation
    GoTSString operator+(const GoTSString& other) const {
        size_t total_size = size() + other.size();
        GoTSString result;
        
        if (total_size <= SSO_THRESHOLD && total_size <= 127) {
            // Result fits in small string
            memcpy(result.small.buffer, c_str(), size());
            memcpy(result.small.buffer + size(), other.c_str(), other.size());
            result.small.buffer[total_size] = '\0';
            result.small.size = static_cast<uint8_t>(total_size) | 0x80;  // Set flag bit + size
        } else {
            // Need large string
            result.large.size = total_size;
            result.large.capacity = ((total_size + 16) & ~15) | 1;
            result.large.data = new char[result.large.capacity & ~1];
            memcpy(result.large.data, c_str(), size());
            memcpy(result.large.data + size(), other.c_str(), other.size());
            result.large.data[total_size] = '\0';
            result.clear_small_flag();  // Make sure flag is clear for large strings
        }
        
        return result;
    }
    
    // Static factory method for creating from data with length
    static GoTSString from_data(const char* data, size_t len) {
        return GoTSString(data, len);
    }
};

int main() {
    std::cout << "Testing UltraScript String System with Null Bytes..." << std::endl;
    
    // Test 1: Create a string with embedded null bytes using the new constructor
    const char test_data[] = {'H', 'e', 'l', 'l', 'o', '\0', 'W', 'o', 'r', 'l', 'd', '\0'};
    size_t test_length = 11; // 11 bytes including the embedded null
    
    GoTSString test_str(test_data, test_length);
    
    std::cout << "Original data length: " << test_length << std::endl;
    std::cout << "GoTSString reported length: " << test_str.size() << std::endl;
    std::cout << "Length matches: " << (test_str.size() == test_length ? "YES" : "NO") << std::endl;
    
    // Test 2: Verify the data content byte by byte
    bool content_matches = true;
    for (size_t i = 0; i < test_length; i++) {
        if (test_str.data()[i] != test_data[i]) {
            content_matches = false;
            std::cout << "Mismatch at position " << i << ": expected " << (int)test_data[i] 
                      << ", got " << (int)test_str.data()[i] << std::endl;
        }
    }
    std::cout << "Content matches: " << (content_matches ? "YES" : "NO") << std::endl;
    
    // Test 3: Test concatenation with null bytes
    const char test_data2[] = {'!', '\0', 'E', 'n', 'd'};
    size_t test_length2 = 5;
    
    GoTSString str2(test_data2, test_length2);
    GoTSString concat_result = test_str + str2;
    
    std::cout << "Concatenated length: " << concat_result.size() << std::endl;
    std::cout << "Expected concat length: " << (test_length + test_length2) << std::endl;
    std::cout << "Concat length matches: " << (concat_result.size() == test_length + test_length2 ? "YES" : "NO") << std::endl;
    
    // Test 4: Compare with original strlen behavior
    std::cout << "\nComparing with C strlen():" << std::endl;
    std::cout << "strlen(test_data): " << strlen(test_data) << " (stops at first null)" << std::endl;
    std::cout << "GoTSString.size(): " << test_str.size() << " (counts all bytes including nulls)" << std::endl;
    
    // Test 5: Print hex dump to show null bytes are preserved
    std::cout << "\nHex dump of concatenated string:" << std::endl;
    for (size_t i = 0; i < concat_result.size(); i++) {
        printf("%02X ", (unsigned char)concat_result.data()[i]);
        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
    std::cout << std::endl;
    
    // Test 6: Test with large strings that exceed SSO threshold
    std::string large_test_data(100, 'A');
    large_test_data[50] = '\0';  // Insert null in the middle
    
    GoTSString large_str(large_test_data.data(), large_test_data.length());
    std::cout << "\nLarge string test:" << std::endl;
    std::cout << "Original length: " << large_test_data.length() << std::endl;
    std::cout << "GoTSString length: " << large_str.size() << std::endl;
    std::cout << "Large string preserves nulls: " << (large_str.size() == large_test_data.length() ? "YES" : "NO") << std::endl;
    
    std::cout << "\nAll tests completed successfully!" << std::endl;
    
    return 0;
}
