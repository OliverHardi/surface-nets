#include "Chunk.h"
#include "World.h"

const glm::ivec3 corners[8] = {
    {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
    {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
};

const int edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

Chunk::~Chunk() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
}


float Chunk::getVoxelWorld(int lx, int ly, int lz) const {
    if ((lx >= 0 && lx < CHUNK_SIZE) && (ly >= 0 && ly < CHUNK_SIZE) && (lz >= 0 && lz < CHUNK_SIZE))
        return voxels[getIndex(lx, ly, lz)];

    // convert to world voxel coord, then to owning chunk coord + local offset
    glm::ivec3 worldVoxel = coord.position * CHUNK_SIZE + glm::ivec3(lx, ly, lz);
    glm::ivec3 ownerChunk = glm::ivec3(glm::floor(glm::vec3(worldVoxel) / float(CHUNK_SIZE)));
    glm::ivec3 localVoxel = worldVoxel - ownerChunk * CHUNK_SIZE;

    auto owner = world->getChunkIfGenerated(ChunkCoord{ownerChunk});

    if (!owner)
        return 0.0f;

    return owner->voxels[owner->getIndex(localVoxel.x, localVoxel.y, localVoxel.z)];
}

void Chunk::generateVoxels(const FastNoiseLite& noise) {
    // initial world generation

    auto startTime = std::chrono::high_resolution_clock::now();


    minValue = std::numeric_limits<float>::max();
    maxValue = std::numeric_limits<float>::lowest();

    glm::vec3 chunkOrigin = static_cast<glm::vec3>(coord.position) * (float)CHUNK_SIZE;

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                glm::vec3 chunkPos = glm::vec3((float)x, (float)y, (float)z);
                glm::vec3 worldPos = chunkOrigin + chunkPos;
                
                glm::vec3 noisePos = worldPos * 1.5f;
                float val = noise.GetNoise(noisePos.x, noisePos.y, noisePos.z);

                // float sdf = worldPos.y - 3.0f; // terrain height
                float sdf = val * 7.0f;
                // float sdf = glm::length(chunkPos - glm::vec3(CHUNK_SIZE * 0.5f)) - 10.0f; // sphere


                voxels[getIndex(x, y, z)] = sdf;

                minValue = std::min(minValue, sdf);
                maxValue = std::max(maxValue, sdf);
            }
        }
    }

    isHomogeneous = (minValue > 0.0f || maxValue < 0.0f);

    // auto endTime = std::chrono::high_resolution_clock::now();
    // std::cout << "Generated chunk at " << coord.position.x << ", " << coord.position.y << ", " << coord.position.z
    //           << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
    //           << " ms\n";
}

//    [ 0, 1, 0, 1, 0, 0]

// [ 1, 0, 1, 0, 1, 0, 0, 0 ]

void Chunk::buildMesh() {

    auto startTime = std::chrono::high_resolution_clock::now();

    vertices.clear();
    indices.clear();

    vertices.reserve(4096);
    indices.reserve(8192);

    constexpr int H = CHUNK_SIZE + 2;
    constexpr int N = CHUNK_SIZE + 1;

    static thread_local std::vector<float> halo(H * H * H);
    static thread_local std::vector<glm::vec3> grad(H * H * H);
    static thread_local std::vector<bool> gradComputed(H * H * H);
    static thread_local std::vector<int> vertexIndex(N * N * N);

    std::fill(gradComputed.begin(), gradComputed.end(), false);
    std::fill(vertexIndex.begin(), vertexIndex.end(), -1);

    auto haloIdx  = [&](int x, int y, int z) { return (x+1)*H*H + (y+1)*H + (z+1); };
    auto gradIdx  = haloIdx; // same shape
    auto vIdxOf   = [&](int x, int y, int z) { return (x+1)*N*N + (y+1)*N + (z+1); };

    for (int x = -1; x < H-1; x++)
    for (int y = -1; y < H-1; y++)
    for (int z = -1; z < H-1; z++)
        halo[haloIdx(x, y, z)] = getVoxelWorld(x, y, z);

    auto getVoxelHalo = [&](int x, int y, int z) -> float {
        return halo[haloIdx(x, y, z)];
    };

    auto getVoxelExtended = [&](int x, int y, int z) -> float {
        if (x >= -1 && x <= CHUNK_SIZE && y >= -1 && y <= CHUNK_SIZE && z >= -1 && z <= CHUNK_SIZE)
            return getVoxelHalo(x, y, z);
        return getVoxelWorld(x, y, z);
    };

    auto getGrad = [&](int x, int y, int z) -> const glm::vec3& {
        int idx = haloIdx(x, y, z); // same shape as halo, so reuse haloIdx
        if (!gradComputed[idx]) {
            grad[idx] = {
                getVoxelExtended(x+1, y, z) - getVoxelExtended(x-1, y, z),
                getVoxelExtended(x, y+1, z) - getVoxelExtended(x, y-1, z),
                getVoxelExtended(x, y, z+1) - getVoxelExtended(x, y, z-1)
            };
            gradComputed[idx] = true;
        }
        return grad[idx];
    };

    auto getVertexIndex = [&](int x, int y, int z) -> int {
        return vertexIndex[vIdxOf(x, y, z)];
    };

    auto addQuad = [&](int a, int b, int c, int d) {
        if (a < 0 || b < 0 || c < 0 || d < 0) return;
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
        indices.push_back(a);
        indices.push_back(c);
        indices.push_back(d);
    };


    for(int x = -1; x < CHUNK_SIZE; x++)
    for(int y = -1; y < CHUNK_SIZE; y++)
    for(int z = -1; z < CHUNK_SIZE; z++)
    {
        float positions[8];
        glm::vec3 gradients[8];
        bool positive = false, negative = false;
        for(int i = 0; i < 8; i++) {
            auto c = corners[i];
            positions[i] = getVoxelHalo(x + c.x, y + c.y, z + c.z);
            gradients[i] = getGrad(x + c.x, y + c.y, z + c.z);
            if(positions[i] < 0.0f) negative = true;
            else                     positive = true;
        }

        if(positive && negative) {
            glm::vec3 positionSum(0.0f);
            int count = 0;

            for(int e = 0; e < 12; e++) {
                int a = edges[e][0];
                int b = edges[e][1];

                float va = positions[a];
                float vb = positions[b];
                if ((va < 0.0f) == (vb < 0.0f)) continue;

                float t = va / (va - vb);
                positionSum += glm::mix(glm::vec3(corners[a]), glm::vec3(corners[b]), t);
                count++;
            }
            
            if(count > 0) {
                glm::vec3 L = positionSum / float(count);
                glm::vec3 I = glm::vec3(1.0f) - L;

                glm::vec3 normal = glm::normalize(
                    I.x*I.y*I.z*gradients[0] + L.x*I.y*I.z*gradients[1] +
                    L.x*L.y*I.z*gradients[2] + I.x*L.y*I.z*gradients[3] +
                    I.x*I.y*L.z*gradients[4] + L.x*I.y*L.z*gradients[5] +
                    L.x*L.y*L.z*gradients[6] + I.x*L.y*L.z*gradients[7]
                );

                // vertexIndex[x+1][y+1][z+1] = static_cast<int>(vertices.size());
                vertexIndex[vIdxOf(x, y, z)] = static_cast<int>(vertices.size());
                vertices.push_back({
                    glm::vec3(x, y, z) + L,
                    normal
                });
            }
        }

    }

    for (int x = 0; x < N-1; x++)
    for (int y = 0; y < N-1; y++)
    for (int z = 0; z < N-1; z++)
    {
        // x edge
        if (y > -1 && z > -1) {
            float a = getVoxelHalo(x, y, z);
            float b = getVoxelHalo(x+1, y, z);

            if ((a < 0) != (b < 0)) {
                int v0 = getVertexIndex(x, y-1, z-1);
                int v1 = getVertexIndex(x, y, z-1);
                int v2 = getVertexIndex(x, y, z);
                int v3 = getVertexIndex(x, y-1, z);

                if (a < b) addQuad(v0, v1, v2, v3);
                else       addQuad(v0, v3, v2, v1);
            }
        }

        // y edge
        if (x > -1 && z > -1) {
            float a = getVoxelHalo(x, y, z);
            float b = getVoxelHalo(x, y+1, z);

            if ((a < 0) != (b < 0)) {
                int v0 = getVertexIndex(x-1, y, z-1);
                int v1 = getVertexIndex(x, y, z-1);
                int v2 = getVertexIndex(x, y, z);
                int v3 = getVertexIndex(x-1, y, z);

                if (a < b) addQuad(v0, v3, v2, v1);
                else       addQuad(v0, v1, v2, v3);
            }
        }

        // z edge
        if (x > -1 && y > -1) {
            float a = getVoxelHalo(x, y, z);
            float b = getVoxelHalo(x, y, z+1);

            if ((a < 0) != (b < 0)) {
                int v0 = getVertexIndex(x-1, y-1, z);
                int v1 = getVertexIndex(x, y-1, z);
                int v2 = getVertexIndex(x, y, z);
                int v3 = getVertexIndex(x-1, y, z);

                if (a < b) addQuad(v0, v1, v2, v3);
                else       addQuad(v0, v3, v2, v1);
            }
        }
    }

    // auto endTime = std::chrono::high_resolution_clock::now();
    // std::cout << "Built mesh for chunk at " << coord.position.x << ", " << coord.position.y << ", " << coord.position.z
    //           << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
    //           << " ms\n";
}

void Chunk::uploadMesh() {

    if (vertices.empty()) {
        state = ChunkState::Ready; // nothing to upload, but mark ready
        return;
    }

    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

    }else{
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    }

    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    state = ChunkState::Ready;
}

void Chunk::draw() {
    // if (state != ChunkState::Ready) { return; }

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
}