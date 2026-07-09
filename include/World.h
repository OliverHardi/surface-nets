#pragma once

#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include <numeric>

#include "Chunk.h"
#include "Shader.h"
#include "Frustum.h"
#include "ThreadPool.h"


class World {

public:
    World();
    ~World();

    void update(glm::vec3 cameraPos, Frustum& frustum);
    void draw(Shader& shader, Frustum& frustum);

    // Chunk* getChunkIfGenerated(ChunkCoord coord) {
    //     auto it = chunks.find(coord);
    //     if (it == chunks.end()) return nullptr;
    //     if (it->second->state == ChunkState::Pending) return nullptr;
    //     return it->second.get();
    // }
    
    std::shared_ptr<Chunk> getChunk(ChunkCoord coord);
    std::shared_ptr<Chunk> getChunkIfGenerated(ChunkCoord coord);


private:

    // ThreadPool threadPool{std::thread::hardware_concurrency() - 2};
    ThreadPool generatePool{std::thread::hardware_concurrency() / 2};
    ThreadPool meshPool{std::thread::hardware_concurrency() / 2};
    
    std::vector<ChunkCoord> generateQueue;
    std::vector<ChunkCoord> meshQueue;

    std::queue<std::shared_ptr<Chunk>> generatedQueue;
    std::mutex generatedMutex;

    std::queue<std::shared_ptr<Chunk>> meshFinishedQueue;
    std::mutex meshFinishedMutex;

    // std::queue<std::shared_ptr<Chunk>> uploadQueue;
    // std::mutex uploadMutex;

    // std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;
    // std::unordered_map<ChunkCoord, std::shared_ptr<Chunk>> chunks;
    std::unordered_map<ChunkCoord, std::shared_ptr<Chunk>, ChunkCoordHash> chunks;
    mutable std::mutex chunksMutex;



    static constexpr int RENDER_DISTANCE = 6;
    static constexpr int VERTICAL_RENDER_DISTANCE = RENDER_DISTANCE;
    // static constexpr int GEN_PER_FRAME    = 2;
    // static constexpr int MESH_PER_FRAME   = 2;

    static constexpr int MAX_GEN_IN_FLIGHT = 16;
    static constexpr int MAX_MESH_IN_FLIGHT = 16;

    void loadChunk(ChunkCoord coord);
    void unloadChunk(ChunkCoord coord);
    // void linkNeighbors(ChunkCoord coord);
    bool readyToMesh(ChunkCoord coord, glm::vec3 cameraPos);
    float priority(ChunkCoord coord, glm::vec3 cameraPos, Frustum& frustum);
    void sortByPriority(std::vector<ChunkCoord>& queue, glm::vec3 cameraPos, Frustum& frustum);
    bool isOutOfRange(ChunkCoord coord, glm::vec3 cameraPos);

    

    FastNoiseLite noise;

};