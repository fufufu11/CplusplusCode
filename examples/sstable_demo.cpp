#include "sstable_builder.h"
#include "value_type.h"
#include <iostream>
#include <filesystem>

int main() {
    std::string test_dir = "./sstable_demo";
    std::string test_file = test_dir + "/demo.sst";
    
    std::filesystem::create_directories(test_dir);
    
    std::cout << "=== SSTable Builder Demo ===" << std::endl;
    std::cout << "Creating SSTable file: " << test_file << std::endl;
    
    {
        SSTableBuilder builder(test_file);
        
        builder.Add("key_001", Value::normal("value_for_key_001"));
        builder.Add("key_002", Value::normal("value_for_key_002"));
        builder.Add("key_003", Value::normal("value_for_key_003"));
        
        for (int i = 4; i <= 100; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = "value_data_" + std::to_string(i);
            builder.Add(key, Value::normal(value));
        }
        
        builder.Finish();
        
        std::cout << "File size: " << builder.FileSize() << " bytes" << std::endl;
        std::cout << "Finished: " << (builder.Finished() ? "yes" : "no") << std::endl;
    }
    
    uint64_t actual_size = std::filesystem::file_size(test_file);
    std::cout << "\n=== File Created ===" << std::endl;
    std::cout << "Path: " << std::filesystem::absolute(test_file) << std::endl;
    std::cout << "Actual size: " << actual_size << " bytes" << std::endl;
    
    return 0;
}
