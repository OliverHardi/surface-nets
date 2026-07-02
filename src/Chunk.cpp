#include "Chunk.h"

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
                
                glm::vec3 noisePos = worldPos * 7.0f;
                float val = noise.GetNoise(noisePos.x, noisePos.y, noisePos.z);

                float sdf = worldPos.y - 3.0f; // terrain height

                float sdf2 = glm::distance(chunkPos, glm::vec3(CHUNK_SIZE/2, CHUNK_SIZE/2, CHUNK_SIZE/2)) - 10.0f; // sphere
                sdf2 += val * 3.0f;

                sdf = std::min(sdf, sdf2) + val * 2.0f;
                // sdf += val * 4.0f;

                voxels[x][y][z] = sdf;

                minValue = std::min(minValue, sdf);
                maxValue = std::max(maxValue, sdf);
            }
        }
    }

    // isHomogeneous = (minValue > 0.0f || maxValue < 0.0f);

    state = ChunkState::Generated;

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Generated chunk at " << coord.position.x << ", " << coord.position.y << ", " << coord.position.z
              << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
              << " ms\n";
}

void Chunk::buildMesh() {

    auto startTime = std::chrono::high_resolution_clock::now();

    vertices.clear();
    indices.clear();

    vertices.reserve(4096);
    indices.reserve(8192);

    constexpr int N = CHUNK_SIZE - 1;

    // precalc gradients
    glm::vec3 grad[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    for (int x = 1; x < N; x++)
    for (int y = 1; y < N; y++)
    for (int z = 1; z < N; z++) {
        grad[x][y][z] = {
            voxels[x+1][y][z] - voxels[x-1][y][z],
            voxels[x][y+1][z] - voxels[x][y-1][z],
            voxels[x][y][z+1] - voxels[x][y][z-1]
        };
    }

    int vertexIndex[N][N][N];
    memset(vertexIndex, -1, sizeof(vertexIndex));

    auto addQuad = [&](int a, int b, int c, int d) {
        if (a < 0 || b < 0 || c < 0 || d < 0) return;
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
        indices.push_back(a);
        indices.push_back(c);
        indices.push_back(d);
    };

    for (int x = 0; x < N; x++)
    for (int y = 0; y < N; y++)
    for (int z = 0; z < N; z++)
    {
        // ---- Vertex placement ----
        float positions[8];
        glm::vec3 gradients[8];
        bool positive = false, negative = false;

        for (int i = 0; i < 8; i++) {
            auto c = corners[i];
            int vx = x + c.x;
            int vy = y + c.y;
            int vz = z + c.z;

            positions[i] = voxels[vx][vy][vz];
            gradients[i] = grad[vx][vy][vz];

            if (positions[i] < 0.0f) negative = true;
            else                     positive = true;
        }

        if (positive && negative) {
            glm::vec3 positionSum(0.0f);
            glm::vec3 normalSum(0.0f);
            int count = 0;

            for (int e = 0; e < 12; e++) {
                int a = edges[e][0];
                int b = edges[e][1];

                float va = positions[a];
                float vb = positions[b];

                if ((va < 0.0f) == (vb < 0.0f)) continue;

                float t = va / (va - vb);
                positionSum += glm::mix(glm::vec3(corners[a]), glm::vec3(corners[b]), t);
                normalSum   += glm::mix(gradients[a], gradients[b], t);
                count++;
            }

            if (count > 0) {
                vertexIndex[x][y][z] = static_cast<int>(vertices.size());
                vertices.push_back({
                    glm::vec3(x, y, z) + positionSum / float(count),
                    normalSum / float(count)
                });
            }
        }

        // ---- X edge (needs y>0, z>0) ----
        if (y > 0 && z > 0) {
            float a = voxels[x][y][z];
            float b = voxels[x+1][y][z];

            if ((a < 0) != (b < 0)) {
                int v0 = vertexIndex[x][y-1][z-1];
                int v1 = vertexIndex[x][y  ][z-1];
                int v2 = vertexIndex[x][y  ][z  ];
                int v3 = vertexIndex[x][y-1][z  ];

                if (a < b) addQuad(v0, v1, v2, v3);
                else       addQuad(v0, v3, v2, v1);
            }
        }

        // ---- Y edge (needs x>0, z>0) ----
        if (x > 0 && z > 0) {
            float a = voxels[x][y][z];
            float b = voxels[x][y+1][z];

            if ((a < 0) != (b < 0)) {
                int v0 = vertexIndex[x-1][y][z-1];
                int v1 = vertexIndex[x  ][y][z-1];
                int v2 = vertexIndex[x  ][y][z  ];
                int v3 = vertexIndex[x-1][y][z  ];

                if (a < b) addQuad(v0, v3, v2, v1);
                else       addQuad(v0, v1, v2, v3);
            }
        }

        // ---- Z edge (needs x>0, y>0) ----
        if (x > 0 && y > 0) {
            float a = voxels[x][y][z];
            float b = voxels[x][y][z+1];

            if ((a < 0) != (b < 0)) {
                int v0 = vertexIndex[x-1][y-1][z];
                int v1 = vertexIndex[x  ][y-1][z];
                int v2 = vertexIndex[x  ][y  ][z];
                int v3 = vertexIndex[x-1][y  ][z];

                if (a < b) addQuad(v0, v1, v2, v3);
                else       addQuad(v0, v3, v2, v1);
            }
        }
    }

    state = ChunkState::Meshed;

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Built mesh for chunk at " << coord.position.x << ", " << coord.position.y << ", " << coord.position.z
              << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
              << " ms\n";
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