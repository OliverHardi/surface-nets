#pragma once
#include <glm/glm.hpp>

struct Frustum {
    glm::vec4 planes[6];

    void extract(const glm::mat4& viewProj) {
        glm::mat4 t = glm::transpose(viewProj); // t[i] now equals row i of viewProj
        planes[0] = t[3] + t[0]; // left
        planes[1] = t[3] - t[0]; // right
        planes[2] = t[3] + t[1]; // bottom
        planes[3] = t[3] - t[1]; // top
        planes[4] = t[3] + t[2]; // near
        planes[5] = t[3] - t[2]; // far
        for (auto& p : planes) p /= glm::length(glm::vec3(p));
    }

    bool intersectsAABB(glm::vec3 min, glm::vec3 max) const {
        for (auto& p : planes) {
            glm::vec3 positive(
                p.x >= 0 ? max.x : min.x,
                p.y >= 0 ? max.y : min.y,
                p.z >= 0 ? max.z : min.z
            );
            if (glm::dot(glm::vec3(p), positive) + p.w < 0)
                return false; // chunk's AABB is entirely outside this plane
        }
        return true;
    }
};