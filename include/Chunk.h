#pragma once

#include <iostream>
#include <chrono>
#include <glm/glm.hpp>
#include <glad/glad.h>

#include "FastNoiseLite.h"

constexpr int CHUNK_SIZE = 32;


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
    Pending,    // exists in map, voxels not yet filled
    Generated,  // voxels filled, waiting for neighbors before meshing
    Meshed,     // mesh built, waiting for GPU upload
    Ready       // fully uploaded, can be drawn
};


class Chunk {
public:
    enum Dir { POS_X, NEG_X, POS_Y, NEG_Y, POS_Z, NEG_Z };

    Chunk() = default;
    ~Chunk();

    ChunkCoord coord{};
    Chunk* neighbors[6] = { nullptr };

    ChunkState state = ChunkState::Pending;

    float sample(int x, int y, int z) const {
        x = std::clamp(x, 0, CHUNK_SIZE - 1);
        y = std::clamp(y, 0, CHUNK_SIZE - 1);
        z = std::clamp(z, 0, CHUNK_SIZE - 1);
        return voxels[x][y][z];
    }

    float sample(glm::ivec3 pos) const {
        return sample(pos.x, pos.y, pos.z);
    }

    void generateVoxels(const FastNoiseLite& noise);
    void buildMesh();
    void uploadMesh();

    void draw();

    bool isHomogeneous = false;

private:
    float voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    float minValue = 0.0f;
    float maxValue = 0.0f;

};