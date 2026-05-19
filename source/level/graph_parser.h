/******************************************************************************
 * @file    graph_parser.h
 * @brief   Navigation graph (.dat) parser for I.G.I. 1
 *
 * Parses binary graph files (e.g. graph1.dat) that contain AI navigation
 * nodes and edges used by the game's pathfinding system.
 *****************************************************************************/

#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Node criteria flags (stored as string names in the file)
inline constexpr int NODECRITERIA_DOOR  = 1;
inline constexpr int NODECRITERIA_VIEW  = 2;
inline constexpr int NODECRITERIA_STAIR = 4;

struct GraphNode {
    int    id       = -1;
    double x        = 0.0;
    double y        = 0.0;
    double z        = 0.0;
    float  gamma    = 0.f;
    float  radius   = 0.f;
    int    material = 0;         // 0-23
    std::string criteria;        // e.g. "NODECRITERIA_VIEW"
    int    link1    = -1;        // connected node ID (-1 = none)
    int    link2    = -1;        // connected node ID (-1 = none)
};

struct GraphEdge {
    int node1     = -1;
    int node2     = -1;
    int link_type = 0;
};

struct GraphFile {
    int                     max_nodes = 0;
    std::vector<GraphNode>  nodes;
    std::vector<GraphEdge>  edges;
    bool                    valid = false;
    std::string             error;
};

// Parse a navigation graph .dat file.
// Filepath example: missions/location0/level1/graphs/graph1.dat
GraphFile GRAPH_Parse(const std::string& filepath);
