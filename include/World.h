#pragma once

#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Chunk.h"
#include "Shader.h"
#include "Frustum.h"


class World {

public:
    World();

    void update(glm::vec3 cameraPos, Frustum& frustum);
    void draw(Shader& shader, Frustum& frustum);

private:
    size_t worldTick = 0;

    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;

    std::vector<ChunkCoord> generateQueue;
    std::vector<ChunkCoord> meshQueue;

    static constexpr int RENDER_DISTANCE = 3;
    static constexpr int GEN_PER_FRAME    = 2;
    static constexpr int MESH_PER_FRAME   = 3;

    void loadChunk(ChunkCoord coord);
    void unloadChunk(ChunkCoord coord);
    void linkNeighbors(ChunkCoord coord);
    bool neighborsReady(Chunk* chunk);
    float priority(ChunkCoord coord, glm::vec3 cameraPos, Frustum& frustum);
    void sortByPriority(std::vector<ChunkCoord>& queue, glm::vec3 cameraPos, Frustum& frustum);

    FastNoiseLite noise;

};