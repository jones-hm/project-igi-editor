#pragma once
#include <string>
#include <functional>

class Compiler {
public:
    Compiler();
    ~Compiler();

    // Compile objects.qsc to objects.qvm
    // Returns true if compilation succeeded, false otherwise
    bool Compile(const std::string& qsc_path, const std::string& qvm_output_path);

    // Set callback for compilation output (for logging)
    void SetOutputCallback(std::function<void(const std::string&)> callback);

private:
    std::function<void(const std::string&)> output_callback_;
};
