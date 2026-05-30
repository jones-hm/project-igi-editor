/******************************************************************************
 * @file    task_schema.cpp
 * @brief   Typed field schema definitions for QSC task objects
 *****************************************************************************/

#include "task_schema.h"

int TypeArgCount(const std::string& typeName) {
    if (typeName == "ObjectPos"   ||
        typeName == "Real32x9"    ||
        typeName == "RGB"         ||
        typeName == "Real32x3"    ||
        typeName == "Real64x3"    ||
        typeName == "Colour") {
        return 3;
    }
    return 1;
}

const std::map<std::string, TaskSchema>& GetBuiltinSchemas() {
    static const std::map<std::string, TaskSchema> s_schemas = {
        { "Building", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "EditRigidObj", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "Terminal", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "HumanSoldier", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Heading",     "Real32",    6,  1 },
            { "Model",       "String16",  7,  1 },
        }},
        { "HumanSoldierFemale", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Heading",     "Real32",    6,  1 },
            { "Model",       "String16",  7,  1 },
        }},
        { "HumanPlayer", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Heading",     "Real32",    6,  1 },
            { "Model",       "String16",  7,  1 },
        }},
        { "Door", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  9,  3 },
            { "Model",       "String16",  12, 1 },
        }},
        { "Car", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Heading",     "Real32",    8,  1 },
            { "Model",       "String16",  13, 1 },
        }},
        { "Heli", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Heading",     "Real32",    8,  1 },
            { "Model",       "String16",  13, 1 },
        }},
        { "ExplodeObject", {
            { "Position",         "ObjectPos", 3,  3 },
            { "Orientation",      "Real32x9",  6,  3 },
            { "Model",            "String16",  9,  1 },
            { "Destroyed model",  "String16",  10, 1 },
        }},
        { "Switch", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  11, 1 },
        }},
        { "Wire", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "AlarmControl", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "SCameraControl", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "SCamera", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  10, 1 },
        }},
        { "SplineObjWaypoint", {
            { "Orientation", "Real32x9",  3,  3 },
            { "Position",    "ObjectPos", 6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "AmbientArea", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
        }},
        { "Train", {
            { "Position",    "TrainPos1D", 3, 1 },
            { "Model",       "String256",  6, 1 },
        }},
        { "Fence", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Heading",     "Real32",    6,  1 },
            { "Model",       "String16",  7,  1 },
        }},
        { "Cabinet", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Heading",     "Real32",    6,  1 },
            { "Model",       "String16",  7,  1 },
        }},
        { "AIStationaryGunHolder", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "AlarmLight", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "Elevator", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "Generator", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "GenericPickup", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "GenericTBA", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "Plane", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "Radio", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "RotatingObject", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "Siren", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "StationaryGun", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "GunPickup", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
        }},
        { "AmmoPickup", {
            { "Position",    "ObjectPos", 3,  3 },
            { "Orientation", "Real32x9",  6,  3 },
            { "Model",       "String16",  9,  1 },
            { "Ammo",        "Int16",     10, 1 },
        }},
    };
    return s_schemas;
}
