#pragma once

#include <glm/glm.hpp>
#include <array>

// ---------------------------------------------------------------------
// Frustum culling - 6 plaszczyzn wyciagnietych z macierzy proj*view
// (metoda Gribb/Hartmann). Kazda plaszczyzna: (a,b,c,d), gdzie
// a*x + b*y + c*z + d >= 0  oznacza "po wewnetrznej stronie".
// ---------------------------------------------------------------------
struct Frustum
{
    std::array<glm::vec4, 6> planes;

    static Frustum fromMatrix(const glm::mat4& m)
    {
        Frustum f;

        // Left
        f.planes[0] = glm::vec4(
            m[0][3] + m[0][0],
            m[1][3] + m[1][0],
            m[2][3] + m[2][0],
            m[3][3] + m[3][0]
        );
        // Right
        f.planes[1] = glm::vec4(
            m[0][3] - m[0][0],
            m[1][3] - m[1][0],
            m[2][3] - m[2][0],
            m[3][3] - m[3][0]
        );
        // Bottom
        f.planes[2] = glm::vec4(
            m[0][3] + m[0][1],
            m[1][3] + m[1][1],
            m[2][3] + m[2][1],
            m[3][3] + m[3][1]
        );
        // Top
        f.planes[3] = glm::vec4(
            m[0][3] - m[0][1],
            m[1][3] - m[1][1],
            m[2][3] - m[2][1],
            m[3][3] - m[3][1]
        );
        // Near
        f.planes[4] = glm::vec4(
            m[0][3] + m[0][2],
            m[1][3] + m[1][2],
            m[2][3] + m[2][2],
            m[3][3] + m[3][2]
        );
        // Far
        f.planes[5] = glm::vec4(
            m[0][3] - m[0][2],
            m[1][3] - m[1][2],
            m[2][3] - m[2][2],
            m[3][3] - m[3][2]
        );

        // normalizacja (dlugosc wektora normalnej a,b,c)
        for (auto& p : f.planes)
        {
            float len = glm::length(glm::vec3(p));
            if (len > 0.00001f)
                p /= len;
        }

        return f;
    }

    // Test AABB (bmin,bmax w przestrzeni swiata) przeciwko frustum.
    // Zwraca true jesli AABB jest (przynajmniej czesciowo) widoczny.
    bool intersectsAABB(const glm::vec3& bmin, const glm::vec3& bmax) const
    {
        for (const auto& p : planes)
        {
            // "positive vertex" - rog AABB najdalej po dodatniej stronie normalnej
            glm::vec3 positive(
                p.x >= 0.f ? bmax.x : bmin.x,
                p.y >= 0.f ? bmax.y : bmin.y,
                p.z >= 0.f ? bmax.z : bmin.z
            );

            float dist = p.x * positive.x + p.y * positive.y + p.z * positive.z + p.w;

            if (dist < 0.f)
                return false; // caly AABB po zlej stronie tej plaszczyzny -> niewidoczny
        }

        return true;
    }
};