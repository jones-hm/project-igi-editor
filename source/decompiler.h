#pragma once
#include <string>
#include <functional>

class Decompiler {
public:
    Decompiler();
    ~Decompiler();

    // Decompile objects.qvm to objects.qsc
    // Returns true if decompilation succeeded, false otherwise
    bool Decompile(const std::string& qvm_path, const std::string& qsc_output_path);

    // Set callback for decompilation output (for logging)
    void SetOutputCallback(std::function<void(const std::string&)> callback);

private:
    std::function<void(const std::string&)> output_callback_;
};
