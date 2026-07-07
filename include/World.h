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

    Chunk* getChunkIfGenerated(ChunkCoord coord) {
        auto it = chunks.find(coord);
        if (it == chunks.end()) return nullptr;
        if (it->second->state == ChunkState::Pending) return nullptr;
        return it->second.get();
    }

private:
    size_t worldTick = 0;

    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;

    std::vector<ChunkCoord> generateQueue;
    std::vector<ChunkCoord> meshCandidates;

    static constexpr int RENDER_DISTANCE = 4;
    static constexpr int VERTICAL_RENDER_DISTANCE = RENDER_DISTANCE;
    static constexpr int GEN_PER_FRAME    = 2;
    static constexpr int MESH_PER_FRAME   = 3;

    void loadChunk(ChunkCoord coord);
    void unloadChunk(ChunkCoord coord);
    void linkNeighbors(ChunkCoord coord);
    bool readyToMesh(ChunkCoord coord, glm::vec3 cameraPos);
    float priority(ChunkCoord coord, glm::vec3 cameraPos, Frustum& frustum);
    void sortByPriority(std::vector<ChunkCoord>& queue, glm::vec3 cameraPos, Frustum& frustum);
    bool isOutOfRange(ChunkCoord coord, glm::vec3 cameraPos);

    

    FastNoiseLite noise;

};