#pragma once

#include <string>

class CLIHandler {
public:
    // Checks if the given arguments contain any of the headless CLI flags
    static bool IsCLICommand(int argc, char** argv);
    
    // Processes the arguments and executes the corresponding parser
    // Returns 0 on success, non-zero on failure
    static int Process(int argc, char** argv);

private:
    static int ParseMEF(const std::string& filepath);
    static int ParseQVM(const std::string& filepath, bool decompile, const std::string& outpath);
    static int CompileQSC(const std::string& inpath, const std::string& outpath);
    static int ParseRES(const std::string& filepath, const std::string& extract_name, const std::string& outpath);
    static int ParseMTP(const std::string& filepath);
    static int ParseTerrain(const std::string& filepath);
    static void PrintHelp();
};