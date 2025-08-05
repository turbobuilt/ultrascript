#include <iostream>
#include <cstring>
#include <vector>
#include <cstdint>

class UltraScriptString {
private:
    char* data_;
    uint64_t length_;
    uint64_t capacity_;
    
    void ensure_capacity(uint64_t new_length) {
        if (new_length > capacity_) {
            uint64_t new_capacity = (new_length + 15) & ~15; // Round up to 16-byte boundary
            char* new_data = new char[new_capacity];
            if (data_ && length_ > 0) {
                memcpy(new_data, data_, length_);
            }
            delete[] data_;
            data_ = new_data;
            capacity_ = new_capacity;
        }
    }
    
public:
    // Default constructor - creates empty string
    UltraScriptString() noexcept : data_(nullptr), length_(0), capacity_(0) {}
    
    // Constructor from C string (null-terminated)
    UltraScriptString(const char* str) : data_(nullptr), length_(0), capacity_(0) {
        if (str) {
            length_ = strlen(str);
            if (length_ > 0) {
                ensure_capacity(length_);
                memcpy(data_, str, length_);
            }
        }
    }
    
    // Constructor from char* with explicit length (no null termination required)
    UltraScriptString(const char* str, uint64_t len) : data_(nullptr), length_(len), capacity_(0) {
        if (str && len > 0) {
            ensure_capacity(len);
            memcpy(data_, str, len);
        }
    }
    
    // Copy constructor
    UltraScriptString(const UltraScriptString& other) : data_(nullptr), length_(other.length_), capacity_(0) {
        if (other.length_ > 0) {
            ensure_capacity(other.length_);
            memcpy(data_, other.data_, other.length_);
        }
    }
    
    // Move constructor
    UltraScriptString(UltraScriptString&& other) noexcept 
        : data_(other.data_), length_(other.length_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.length_ = 0;
        other.capacity_ = 0;
    }
    
    // Assignment operators
    UltraScriptString& operator=(const UltraScriptString& other) {
        if (this != &other) {
            delete[] data_;
            data_ = nullptr;
            length_ = other.length_;
            capacity_ = 0;
            if (other.length_ > 0) {
                ensure_capacity(other.length_);
                memcpy(data_, other.data_, other.length_);
            }
        }
        return *this;
    }
    
    UltraScriptString& operator=(UltraScriptString&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            length_ = other.length_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.length_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }
    
    // Destructor
    ~UltraScriptString() {
        delete[] data_;
    }
    
    // Access methods
    inline const char* data() const { return data_; }
    inline uint64_t length() const { return length_; }
    inline uint64_t size() const { return length_; }
    inline bool empty() const { return length_ == 0; }
    
    // Character access
    inline char operator[](uint64_t index) const {
        return (index < length_) ? data_[index] : '\0';
    }
    
    // Get null-terminated C string (creates temporary copy)
    const char* c_str() const {
        if (length_ == 0) return "";
        
        // Create a null-terminated copy
        static thread_local std::vector<char> temp_buffer;
        temp_buffer.resize(length_ + 1);
        memcpy(temp_buffer.data(), data_, length_);
        temp_buffer[length_] = '\0';
        return temp_buffer.data();
    }
    
    // Concatenation
    UltraScriptString operator+(const UltraScriptString& other) const {
        uint64_t total_length = length_ + other.length_;
        UltraScriptString result;
        if (total_length > 0) {
            result.ensure_capacity(total_length);
            result.length_ = total_length;
            if (length_ > 0) {
                memcpy(result.data_, data_, length_);
            }
            if (other.length_ > 0) {
                memcpy(result.data_ + length_, other.data_, other.length_);
            }
        }
        return result;
    }
    
    // Comparison operators
    bool operator==(const UltraScriptString& other) const {
        if (length_ != other.length_) return false;
        if (length_ == 0) return true;
        return memcmp(data_, other.data_, length_) == 0;
    }
    
    bool operator!=(const UltraScriptString& other) const {
        return !(*this == other);
    }
};

int main() {
    std::cout << "=== UltraScript String Test ===" << std::endl;
    
    // Test 1: Basic string creation
    std::cout << "\n1. Testing basic string creation..." << std::endl;
    UltraScriptString str1("Hello");
    UltraScriptString str2("World");
    
    std::cout << "str1: '" << std::string(str1.data(), str1.length()) << "' (length: " << str1.length() << ")" << std::endl;
    std::cout << "str2: '" << std::string(str2.data(), str2.length()) << "' (length: " << str2.length() << ")" << std::endl;
    
    // Test 2: String concatenation
    std::cout << "\n2. Testing string concatenation..." << std::endl;
    UltraScriptString str3 = str1 + str2;
    std::cout << "str1 + str2: '" << std::string(str3.data(), str3.length()) << "' (length: " << str3.length() << ")" << std::endl;
    
    // Test 3: String comparison
    std::cout << "\n3. Testing string comparison..." << std::endl;
    UltraScriptString str4("Hello");
    std::cout << "str1 == str4: " << (str1 == str4 ? "true" : "false") << std::endl;
    std::cout << "str1 == str2: " << (str1 == str2 ? "true" : "false") << std::endl;
    
    // Test 4: Empty string
    std::cout << "\n4. Testing empty string..." << std::endl;
    UltraScriptString empty;
    std::cout << "empty.length(): " << empty.length() << std::endl;
    std::cout << "empty.empty(): " << (empty.empty() ? "true" : "false") << std::endl;
    
    // Test 5: String with length constructor (no null termination needed)
    std::cout << "\n5. Testing string with explicit length..." << std::endl;
    const char* data = "Hello\0World"; // Contains null byte
    UltraScriptString str5(data, 11); // Include the null byte and "World"
    std::cout << "str5 length: " << str5.length() << std::endl;
    std::cout << "str5 data: ";
    for (uint64_t i = 0; i < str5.length(); i++) {
        char c = str5[i];
        if (c == '\0') {
            std::cout << "\\0";
        } else {
            std::cout << c;
        }
    }
    std::cout << std::endl;
    
    // Test 6: c_str() function (creates null-terminated copy)
    std::cout << "\n6. Testing c_str() function..." << std::endl;
    std::cout << "str1.c_str(): '" << str1.c_str() << "'" << std::endl;
    
    // Test 7: Character access
    std::cout << "\n7. Testing character access..." << std::endl;
    std::cout << "str1[0]: '" << str1[0] << "'" << std::endl;
    std::cout << "str1[4]: '" << str1[4] << "'" << std::endl;
    std::cout << "str1[10] (out of bounds): '" << str1[10] << "' (should be \\0)" << std::endl;
    
    std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    return 0;
}
