/******************************************************************************
 * @file    task_schema.h
 * @brief   Typed field schema definitions for QSC task objects
 *****************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <map>

struct FieldDef {
    std::string name;       // Display name
    std::string typeName;   // Type name (ObjectPos, Real32, String16, etc.)
    int         argOffset;  // Index into argTokens for first value
    int         argCount;   // Number of argTokens consumed by this field
};

using TaskSchema = std::vector<FieldDef>;

// Returns the number of argToken slots a given type name consumes
int TypeArgCount(const std::string& typeName);

// Returns a map of type-name -> schema.
// Caller must not mutate the returned map.
const std::map<std::string, TaskSchema>& GetBuiltinSchemas();
