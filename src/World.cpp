#include "World.h"

World::World() {
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
}

static const glm::ivec3 kNeighborOffsets[6] = {
    { 1, 0, 0}, {-1, 0, 0},
    { 0, 1, 0}, { 0,-1, 0},
    { 0, 0, 1}, { 0, 0,-1}
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
    chunk->coord = coord;
    chunks[coord] = std::move(chunk);
    linkNeighbors(coord);
    generateQueue.push_back(coord);
}

void World::unloadChunk(ChunkCoord coord) {
    auto it = chunks.find(coord);
    if (it == chunks.end()) return;

    Chunk* chunk = it->second.get();
    for (int i = 0; i < 6; i++) {
        if (chunk->neighbors[i]) {
            chunk->neighbors[i]->neighbors[i ^ 1] = nullptr;
            chunk->neighbors[i] = nullptr;
        }
    }

    chunks.erase(it);

    // Remove from any pending queues (cheap since queues are small)
    auto removeFromQueue = [&](std::vector<ChunkCoord>& q) {
        q.erase(std::remove(q.begin(), q.end(), coord), q.end());
    };
    removeFromQueue(generateQueue);
    removeFromQueue(meshQueue);
}

void World::linkNeighbors(ChunkCoord coord) {
    Chunk* self = chunks[coord].get();

    for (int i = 0; i < 6; i++) {
        ChunkCoord neighborCoord{ coord.position + kNeighborOffsets[i] };
        auto it = chunks.find(neighborCoord);
        if (it != chunks.end()) {
            Chunk* neighbor = it->second.get();
            self->neighbors[i] = neighbor;
            neighbor->neighbors[i ^ 1] = self;
        }
    }
}

bool World::neighborsReady(Chunk* chunk) {
    for (int i = 0; i < 6; i++) {
        Chunk* neighbor = chunk->neighbors[i];
        if (neighbor && neighbor->state == ChunkState::Pending)
            return false;
    }
    return true;
}


void World::update(glm::vec3 cameraPos, Frustum& frustum) {

    // --- Spawn chunks in range ---
    glm::ivec3 center = glm::ivec3(glm::floor(cameraPos / float(CHUNK_SIZE)));

    for (int x = -RENDER_DISTANCE; x <= RENDER_DISTANCE; x++)
    for (int y = -RENDER_DISTANCE; y <= RENDER_DISTANCE; y++)
    for (int z = -RENDER_DISTANCE; z <= RENDER_DISTANCE; z++) {
        ChunkCoord coord{ center + glm::ivec3(x, y, z) };
        if (chunks.find(coord) == chunks.end())
            loadChunk(coord);
    }

    // --- Despawn chunks out of range ---
    std::vector<ChunkCoord> toUnload;
    for (auto& [coord, chunk] : chunks) {
        glm::ivec3 delta = glm::abs(coord.position - center);
        if (delta.x > RENDER_DISTANCE || delta.y > RENDER_DISTANCE || delta.z > RENDER_DISTANCE)
            toUnload.push_back(coord);
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
            auto alreadyQueued = [&](std::vector<ChunkCoord>& q, ChunkCoord c) {
                return std::find(q.begin(), q.end(), c) != q.end();
            };
            for (int j = 0; j < 6; j++) {
                Chunk* neighbor = chunk->neighbors[j];
                if (neighbor && neighbor->state != ChunkState::Pending){
                    if (!alreadyQueued(meshQueue, neighbor->coord))
                        meshQueue.emplace(meshQueue.begin(), neighbor->coord);
                }
            }
            if (!alreadyQueued(meshQueue, coord))
                meshQueue.emplace(meshQueue.begin(), coord);
            
        }
        
    }

    // --- Mesh ---
    if (worldTick == 0 && !meshQueue.empty()) {
        sortByPriority(meshQueue, cameraPos, frustum);
    }
    for (int i = 0; i < MESH_PER_FRAME && !meshQueue.empty(); i++) {
        ChunkCoord coord = meshQueue.back();
        meshQueue.pop_back();

        auto it = chunks.find(coord);
        if (it == chunks.end()) continue;

        Chunk* chunk = it->second.get();
        if (chunk->state == ChunkState::Pending) continue;
        if (!neighborsReady(chunk)) continue;

        chunk->buildMesh();
        chunk->uploadMesh();
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