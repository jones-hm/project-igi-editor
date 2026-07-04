#pragma once
#include <string>
#include <unordered_set>
#include "../renderer/res_writer.h"

// Set of model ids (NNN_NN_N) packed as <id>.mef entries inside a level .res.
// Used to warn when an object references a model the game archive lacks.
struct CaseInsensitiveHash {
    size_t operator()(const std::string& str) const {
        // FNV-1a hash
        size_t h = 14695981039346656037ull;
        for (char c : str) {
            h ^= static_cast<size_t>(std::tolower(static_cast<unsigned char>(c)));
            h *= 1099511628211ull;
        }
        return h;
    }
};

struct CaseInsensitiveEqual {
    bool operator()(const std::string& a, const std::string& b) const {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }
        return true;
    }
};

class ResModelSet {
public:
    ResModelSet() = default;
    explicit ResModelSet(const RESFile& res);
    // Add a single .res entry by name (e.g. "models\\426_02_1.mef"). Non-.mef
    // entries are ignored. Lets callers stream names without materializing a RESFile.
    void AddEntry(const std::string& entryName);
    bool Contains(const std::string& modelId) const;
    bool Empty() const { return ids_.empty(); }
private:
    std::unordered_set<std::string, CaseInsensitiveHash, CaseInsensitiveEqual> ids_; // lower-cased model ids
};
