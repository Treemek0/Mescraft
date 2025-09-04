#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Plane {
    glm::vec3 normal;
    float d;
};

struct Frustum {
    Plane planes[6];
};

// extract frustum planes from projection * view matrix
Frustum extractFrustum(const glm::mat4& projView) {
    Frustum f;

    // Left
    f.planes[0].normal.x = projView[0][3] + projView[0][0];
    f.planes[0].normal.y = projView[1][3] + projView[1][0];
    f.planes[0].normal.z = projView[2][3] + projView[2][0];
    f.planes[0].d        = projView[3][3] + projView[3][0];

    // Right
    f.planes[1].normal.x = projView[0][3] - projView[0][0];
    f.planes[1].normal.y = projView[1][3] - projView[1][0];
    f.planes[1].normal.z = projView[2][3] - projView[2][0];
    f.planes[1].d        = projView[3][3] - projView[3][0];

    // Bottom
    f.planes[2].normal.x = projView[0][3] + projView[0][1];
    f.planes[2].normal.y = projView[1][3] + projView[1][1];
    f.planes[2].normal.z = projView[2][3] + projView[2][1];
    f.planes[2].d        = projView[3][3] + projView[3][1];

    // Top
    f.planes[3].normal.x = projView[0][3] - projView[0][1];
    f.planes[3].normal.y = projView[1][3] - projView[1][1];
    f.planes[3].normal.z = projView[2][3] - projView[2][1];
    f.planes[3].d        = projView[3][3] - projView[3][1];

    // Near
    f.planes[4].normal.x = projView[0][3] + projView[0][2];
    f.planes[4].normal.y = projView[1][3] + projView[1][2];
    f.planes[4].normal.z = projView[2][3] + projView[2][2];
    f.planes[4].d        = projView[3][3] + projView[3][2];

    // Far
    f.planes[5].normal.x = projView[0][3] - projView[0][2];
    f.planes[5].normal.y = projView[1][3] - projView[1][2];
    f.planes[5].normal.z = projView[2][3] - projView[2][2];
    f.planes[5].d        = projView[3][3] - projView[3][2];

    // normalize planes
    for(int i=0; i<6; i++) {
        float len = glm::length(f.planes[i].normal);
        f.planes[i].normal /= len;
        f.planes[i].d /= len;
    }

    return f;
}

bool isBoxInFrustum(const Frustum& f, const glm::vec3& min, const glm::vec3& max) {
    for(int i=0; i<6; i++) {
        glm::vec3 positive = min;
        if(f.planes[i].normal.x >= 0) positive.x = max.x;
        if(f.planes[i].normal.y >= 0) positive.y = max.y;
        if(f.planes[i].normal.z >= 0) positive.z = max.z;

        if(glm::dot(f.planes[i].normal, positive) + f.planes[i].d < 0) {
            return false; // completely outside
        }
    }
    return true;
}