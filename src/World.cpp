#include "World.h"

World::World() {
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
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

void World::sortByPriority(std::vector<ChunkCoord>& queue, glm::vec3 cameraPos, Frustum& frustum) {
    // Sort descending so the lowest-priority (closest) chunk is at .back(),
    // letting us pop_back() in O(1) instead of erasing from the front.
    std::sort(queue.begin(), queue.end(),
        [&](const ChunkCoord& a, const ChunkCoord& b) {
            return priority(a, cameraPos, frustum) > priority(b, cameraPos, frustum);
        });
}

void World::loadChunk(ChunkCoord coord) {
    auto chunk = std::make_unique<Chunk>();
    chunk->world = this;
    chunk->coord = coord;
    chunks[coord] = std::move(chunk);
    linkNeighbors(coord);
    generateQueue.push_back(coord);
}

void World::unloadChunk(ChunkCoord coord) {
    auto it = chunks.find(coord);
    if (it == chunks.end()) return;

    for (int i = 0; i < 12; i++) {
        ChunkCoord neighborCoord{ coord.position + kOffsets[i] };

        auto it = chunks.find(neighborCoord);
        if (it != chunks.end())
            it->second->neighbors[i] = nullptr;
    }

    chunks.erase(it);

    // Remove from any pending queues (cheap since queues are small)
    auto removeFromQueue = [&](std::vector<ChunkCoord>& q) {
        q.erase(std::remove(q.begin(), q.end(), coord), q.end());
    };
    removeFromQueue(generateQueue);
    removeFromQueue(meshCandidates);
}

void World::linkNeighbors(ChunkCoord coord) {
    Chunk* self = chunks[coord].get();

    for (int i = 0; i < 12; i++) {
        // self's own positive-direction neighbor, if it exists
        ChunkCoord neighborCoord{ coord.position + kOffsets[i] };
        auto it = chunks.find(neighborCoord);
        if (it != chunks.end()) {
            self->neighbors[i] = it->second.get();
            it->second->neighbors[i] = self;
        }
    }
}


bool World::readyToMesh(ChunkCoord coord, glm::vec3 cameraPos) {
    for( auto offset : kOffsets ) {
        ChunkCoord neighborCoord{ coord.position + offset };
        if (isOutOfRange(neighborCoord, cameraPos)) {
            continue; // out of bounds, ignore
        }
        auto it = chunks.find(neighborCoord);
        if (it == chunks.end() || it->second->state == ChunkState::Pending) {
            return false; // neighbor not generated yet
        }
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
        if (chunks.find(coord) == chunks.end())
            loadChunk(coord);
    }

    // --- Despawn chunks out of range ---
    std::vector<ChunkCoord> toUnload;
    for (auto& [coord, chunk] : chunks) {
        if (isOutOfRange(coord, cameraPos)) {
            toUnload.push_back(coord);
        }
    }
    for (auto& coord : toUnload)
        unloadChunk(coord);

    // --- Generate ---
    if (worldTick == 0 && !generateQueue.empty()) {
        sortByPriority(generateQueue, cameraPos, frustum);
    }
    for (int i = 0; i < GEN_PER_FRAME && !generateQueue.empty(); i++) {
        ChunkCoord coord = generateQueue.back();
        generateQueue.pop_back();

        auto it = chunks.find(coord);
        if (it == chunks.end()) continue;

        Chunk* chunk = it->second.get();
        chunk->generateVoxels(noise);

        if(chunk->isHomogeneous) {
            chunk->state = ChunkState::Ready;
        } else {
            meshCandidates.push_back(coord);
            for (int i = 0; i < 12; i++) {
                ChunkCoord neighborCoord{ coord.position + kOffsets[i] };
                auto it = chunks.find(neighborCoord);
                if (it == chunks.end()) continue;

                auto state = it->second->state;
                if (state == ChunkState::Generated ||
                    state == ChunkState::Meshed ||
                    state == ChunkState::Ready) {
                    meshCandidates.push_back(neighborCoord);
                    // it->second->state = ChunkState::Generated;
                }
            }
        }
        
    }

    // for each mesh candidate
    int chunksMeshed = 0;

    for(auto& coord : meshCandidates) {
        // check if neighbors are either generated or out of bounds
        if(readyToMesh(coord, cameraPos)) {
            auto it = chunks.find(coord);
            if (it == chunks.end()) continue;

            Chunk* chunk = it->second.get();
            chunk->buildMesh();
            chunk->uploadMesh();
            chunk->state = ChunkState::Ready;
            // remove from mesh candidates
            meshCandidates.erase(std::remove(meshCandidates.begin(), meshCandidates.end(), coord), meshCandidates.end());
            chunksMeshed++;
            if(chunksMeshed >= MESH_PER_FRAME) {
                break; // only mesh a limited number of chunks per frame
            }
        }
    }


    worldTick = (worldTick + 1) % 30;
}

void World::draw(Shader& shader, Frustum& frustum) {
    for (auto& [coord, chunk] : chunks) {
        if (chunk->state != ChunkState::Ready){ continue; } // only draw uploaded chunks

        // aabb
        glm::vec3 chunkMin = glm::vec3(coord.position * CHUNK_SIZE);
        glm::vec3 chunkMax = chunkMin + glm::vec3(CHUNK_SIZE);
        if (!frustum.intersectsAABB(chunkMin, chunkMax)){ continue; }

        glm::mat4 model = glm::translate(glm::mat4(1.0f),
            glm::vec3(coord.position * CHUNK_SIZE));
        shader.setMat4("model", model);
        chunk->draw();
    }
}

/*



chunk is created and added to the generate queue
marked as pending

generate queue is simple, just partial sort by recomputed priority every few frames
and generate n chunks per frame
later move this to worker threads

after a chunk is generated
mark as generated instead of pending
add to array of chunks to mesh

every frame each of the generated chunks check the neighbors they rely on if they've been generated
if they have, mesh the chunk and upload
mark as ready

once n chunks have been meshed in a single check, break out of the loop


*/