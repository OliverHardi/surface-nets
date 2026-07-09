#include "World.h"

World::World() {
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
}

World::~World()
{
    generatePool.shutdown();
    meshPool.shutdown();
}

static const glm::ivec3 kOffsets[12] = {
    { 1,  0,  0}, // +X
    { 0,  1,  0}, // +Y
    { 0,  0,  1}, // +Z
    { 1,  1,  0}, // +X +Y
    { 1,  0,  1}, // +X +Z
    { 0,  1,  1}, // +Y +Z
    {-1,  0,  0}, // -X
    { 0, -1,  0}, // -Y
    { 0,  0, -1}, // -Z
    {-1, -1,  0}, // -X -Y
    {-1,  0, -1}, // -X -Z
    { 0, -1, -1}, // -Y -Z
};


std::shared_ptr<Chunk> World::getChunk(ChunkCoord coord) {
    std::lock_guard lock(chunksMutex);

    auto it = chunks.find(coord);

    if (it == chunks.end())
        return nullptr;

    return it->second;
}

std::shared_ptr<Chunk> World::getChunkIfGenerated(ChunkCoord coord) {
    std::lock_guard<std::mutex> lock(chunksMutex);

    auto it = chunks.find(coord);

    if (it == chunks.end())
        return nullptr;

    auto chunk = it->second;

    if (chunk->state == ChunkState::Pending ||
        chunk->state == ChunkState::Generating
        // || chunk->state == ChunkState::Meshing
    ) {
        return nullptr;
    }

    return chunk;
}

float World::priority(ChunkCoord coord, glm::vec3 cameraPos, Frustum& frustum) {
    glm::vec3 chunkCenter = glm::vec3(coord.position) * float(CHUNK_SIZE)
                           + glm::vec3(CHUNK_SIZE * 0.5f);

    float dist = glm::length(chunkCenter - cameraPos);

    glm::vec3 chunkMin = glm::vec3(coord.position * CHUNK_SIZE);
    glm::vec3 chunkMax = chunkMin + glm::vec3(CHUNK_SIZE);
    if (!frustum.intersectsAABB(chunkMin, chunkMax)) {
        dist *= 1.5f; // bias
    }
    return dist;
}

// void World::sortByPriority(std::vector<ChunkCoord>& queue, glm::vec3 cameraPos, Frustum& frustum) {
//     std::sort(queue.begin(), queue.end(),
//         [&](const ChunkCoord& a, const ChunkCoord& b) {
//             return priority(a, cameraPos, frustum) > priority(b, cameraPos, frustum);
//         });
// }

void World::sortByPriority(std::vector<ChunkCoord>& queue, glm::vec3 cameraPos, Frustum& frustum) {
    std::vector<float> priorities(queue.size());
    for (size_t i = 0; i < queue.size(); i++)
        priorities[i] = priority(queue[i], cameraPos, frustum);

    std::vector<size_t> indices(queue.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::sort(indices.begin(), indices.end(),
        [&](size_t a, size_t b) { return priorities[a] > priorities[b]; });

    std::vector<ChunkCoord> sorted(queue.size());
    for (size_t i = 0; i < queue.size(); i++)
        sorted[i] = queue[indices[i]];
    queue = std::move(sorted);
}

void World::loadChunk(ChunkCoord coord) {
    auto chunk = std::make_shared<Chunk>();
    chunk->world = this;
    chunk->coord = coord;

    // chunks[coord] = chunk;
    {
        std::lock_guard<std::mutex> lock(chunksMutex);
        chunks[coord] = chunk;
    }

    generateQueue.push_back(coord);
}

void World::unloadChunk(ChunkCoord coord) {
    // auto it = chunks.find(coord);
    // if (it == chunks.end()) return;

    // chunks.erase(it);
    {
        std::lock_guard<std::mutex> lock(chunksMutex);

        auto it = chunks.find(coord);
        if (it == chunks.end())
            return;

        chunks.erase(it);
    }

    // Remove from any pending queues (cheap since queues are small)
    auto removeFromQueue = [&](std::vector<ChunkCoord>& q) {
        q.erase(std::remove(q.begin(), q.end(), coord), q.end());
    };
    removeFromQueue(generateQueue);
    removeFromQueue(meshQueue);
}


bool World::readyToMesh(ChunkCoord coord, glm::vec3 cameraPos) {
    for(auto offset : kOffsets) {
        ChunkCoord neighborCoord{coord.position + offset};

        if(isOutOfRange(neighborCoord, cameraPos))
            continue;

        if(!getChunkIfGenerated(neighborCoord))
            return false;
    }

    return true;
}

bool World::isOutOfRange(ChunkCoord coord, glm::vec3 cameraPos) {
    glm::ivec3 center = glm::ivec3(glm::floor(cameraPos / float(CHUNK_SIZE)));
    glm::ivec3 delta = glm::abs(coord.position - center);
    return delta.x > RENDER_DISTANCE || delta.y > VERTICAL_RENDER_DISTANCE || delta.z > RENDER_DISTANCE;
}

void World::update(glm::vec3 cameraPos, Frustum& frustum) {

    // --- Spawn chunks in range ---
    glm::ivec3 center = glm::ivec3(glm::floor(cameraPos / float(CHUNK_SIZE)));

    for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; x++)
    for (int y = -VERTICAL_RENDER_DISTANCE; y <= VERTICAL_RENDER_DISTANCE; y++)
    for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; z++) {
        ChunkCoord coord{ center + glm::ivec3(x, y, z) };
        bool exists;
        {
            std::lock_guard lock(chunksMutex);
            exists = chunks.find(coord) != chunks.end();
        }
        if (!exists)
            loadChunk(coord);
    }

    // --- Despawn chunks out of range ---
    std::vector<ChunkCoord> toUnload;
    {
        std::lock_guard lock(chunksMutex);

        for (auto& [coord, chunk] : chunks)
        {
            if (isOutOfRange(coord, cameraPos))
                toUnload.push_back(coord);
        }
    }
    for (auto& coord : toUnload)
        unloadChunk(coord);



    std::cout << "nums: " << generatePool.pendingCount() << ", " << meshPool.pendingCount() << std::endl;

    // --- Generate ---
    if (!generateQueue.empty()) {
        sortByPriority(generateQueue, cameraPos, frustum);
    }

    for (int i = 0;
        !generateQueue.empty() && generatePool.pendingCount() < MAX_GEN_IN_FLIGHT; i++) {
    // for (int i = 0; !generateQueue.empty(); i++) {
        ChunkCoord coord = generateQueue.back();
        generateQueue.pop_back();

        auto chunk = getChunk(coord);

        if (!chunk)
            continue;

        chunk->state = ChunkState::Generating;

        generatePool.submit([this, chunk]
        {
            chunk->generateVoxels(noise);

            std::lock_guard lock(generatedMutex);
            generatedQueue.push(chunk);
        });
    }

    {
        std::lock_guard<std::mutex> lock(generatedMutex);

        while (!generatedQueue.empty()) {
            auto chunk = generatedQueue.front();
            generatedQueue.pop();

            if (chunk->isHomogeneous)
            {
                chunk->state = ChunkState::Ready;
            }
            else
            {
                auto pushMeshCandidate = [&](ChunkCoord c) {
                    if (std::find(meshQueue.begin(), meshQueue.end(), c) == meshQueue.end())
                        meshQueue.push_back(c);
                };

                chunk->state = ChunkState::Generated;

                pushMeshCandidate(chunk->coord);

                for (int i = 0; i < 12; i++)
                {
                    ChunkCoord neighborCoord{ chunk->coord.position + kOffsets[i] };

                    auto neighbor = getChunkIfGenerated(neighborCoord);

                    if (!neighbor)
                        continue;

                    if (neighbor->state == ChunkState::Generated ||
                        neighbor->state == ChunkState::Meshing || 
                        neighbor->state == ChunkState::MeshReady ||
                        neighbor->state == ChunkState::Ready
                    ) {
                        pushMeshCandidate(neighbor->coord);
                    }
                }
            }
        }
    }

    // --- Start meshing jobs ---

    if (!meshQueue.empty()) {
        sortByPriority(meshQueue, cameraPos, frustum);
    }
    int chunksStarted = 0;

    for (size_t i = 0; 
        i < meshQueue.size() && meshPool.pendingCount() < MAX_MESH_IN_FLIGHT;)
    {
        ChunkCoord coord = meshQueue[i];

        if (readyToMesh(coord, cameraPos))
        {
            auto chunk = getChunk(coord);

            if (chunk && chunk->state != ChunkState::Meshing)
            {
                chunk->state = ChunkState::Meshing;

                meshPool.submit([this, chunk]
                {
                    chunk->buildMesh();

                    chunk->state = ChunkState::MeshReady;

                    std::lock_guard lock(meshFinishedMutex);
                    meshFinishedQueue.push(chunk);
                });
            }

            meshQueue.erase(meshQueue.begin() + i);
            ++chunksStarted;
        }
        else
        {
            ++i;
        }
    }

    // --- Upload finished meshes ---
    {
        std::lock_guard lock(meshFinishedMutex);

        while (!meshFinishedQueue.empty())
        {
            auto chunk = meshFinishedQueue.front();
            meshFinishedQueue.pop();

            chunk->uploadMesh();
            chunk->state = ChunkState::Ready;
        }
    }

}




// void World::draw(Shader& shader, Frustum& frustum) {
//     for (auto& [coord, chunk] : chunks) {
//         if (chunk->state != ChunkState::Ready){ continue; } // only draw uploaded chunks

//         // aabb
//         glm::vec3 chunkMin = glm::vec3(coord.position * CHUNK_SIZE);
//         glm::vec3 chunkMax = chunkMin + glm::vec3(CHUNK_SIZE);
//         if (!frustum.intersectsAABB(chunkMin, chunkMax)){ continue; }

//         glm::mat4 model = glm::translate(glm::mat4(1.0f),
//             glm::vec3(coord.position * CHUNK_SIZE));
//         shader.setMat4("model", model);
//         chunk->draw();
//     }
// }

void World::draw(Shader& shader, Frustum& frustum)
{
    std::vector<std::pair<ChunkCoord, std::shared_ptr<Chunk>>> visibleChunks;

    {
        std::lock_guard lock(chunksMutex);

        for (auto& [coord, chunk] : chunks)
            visibleChunks.push_back({coord, chunk});
    }

    for (auto& [coord, chunk] : visibleChunks)
    {
        if (chunk->state != ChunkState::Ready)
            continue;

        glm::vec3 chunkMin = glm::vec3(coord.position * CHUNK_SIZE);
        glm::vec3 chunkMax = chunkMin + glm::vec3(CHUNK_SIZE);

        if (!frustum.intersectsAABB(chunkMin, chunkMax))
            continue;

        glm::mat4 model = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(coord.position * CHUNK_SIZE)
        );

        shader.setMat4("model", model);
        chunk->draw();
    }
}