#pragma once

#include <iostream>
#include <chrono>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <atomic>

#include "FastNoiseLite.h"

constexpr int CHUNK_SIZE = 32;

class World;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct ChunkCoord {
    glm::ivec3 position;
    bool operator==(const ChunkCoord& o) const { return position == o.position; }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        size_t h = std::hash<int>()(c.position.x);
        h ^= std::hash<int>()(c.position.y) + 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<int>()(c.position.z) + 0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};

enum class ChunkState {
    Pending,
    Generating,
    Generated,
    Meshing,
    MeshReady,
    Ready
};


class Chunk {
public:
    enum Dir { POS_X, POS_Y, POS_Z, POS_XY, POS_XZ, POS_YZ, NEG_X, NEG_Y, NEG_Z, NEG_XY, NEG_XZ, NEG_YZ };

    Chunk() = default;
    ~Chunk();

    ChunkCoord coord{};

    // ChunkState state = ChunkState::Pending;
    std::atomic<ChunkState> state{ChunkState::Pending};
    
    World* world = nullptr;

    void generateVoxels(const FastNoiseLite& noise);
    void buildMesh();
    void uploadMesh();

    void draw();

    bool isHomogeneous = false;

    inline int getIndex(int x, int y, int z) const {
        if constexpr (CHUNK_SIZE == 32) {
            return (x << 10) | (y << 5) | z;
        } else {
            return x * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + z;
        }
    }

    inline float getVoxel(int x, int y, int z) const {
        return voxels[getIndex(x, y, z)];
    }

    float getVoxelWorld(int lx, int ly, int lz) const;

private:
    float voxels[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE]; // flat array for better cache locality

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    float minValue = 0.0f;
    float maxValue = 0.0f;

};



