#pragma once
#include <string>
#include <vector>
#include <map>

// Shared QSC task-parameter schema definitions.
//
// Extracted from verify_level.cpp so that both the level verifier and the
// in-editor typed property UI resolve a task type's parameter layout from a
// single source of truth.
namespace TaskSchemaNS {

struct FieldDef {
    std::string name;
    std::string typeName;
    int argOffset = 0;  // absolute index in the Task_New arg list (0 = taskId)
    int argCount  = 1;  // number of Task_New args this field consumes
};
using TaskSchema = std::vector<FieldDef>;

// How many Task_New args one field of a given type consumes (QSC wire format).
// Real32x9 is a 3x3 rotation matrix at runtime but is serialised as 3 Euler
// angles in QSC; all other multi-float types follow their obvious count.
int TypeArgCount(const std::string& typeName);

// Hardcoded schemas for all known IGI task types.  Task_DeclareParameters is a
// compile-time directive stripped from QVM, so QVM-decompiled files never
// contain it; these mirror what the game engine defines.
const std::map<std::string, TaskSchema>& GetBuiltinSchemas();

// Schema for one task type, or nullptr if no builtin schema exists.
const TaskSchema* GetBuiltinSchema(const std::string& taskType);

} // namespace TaskSchemaNS
