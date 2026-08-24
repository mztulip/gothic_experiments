#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <random>
#include <algorithm>
#include "PfxParams.hpp"
#include "Camera.hpp"

struct LiveParticle {
    glm::vec3 pos;
    glm::vec3 vel;
    float age = 0.f;
    float lifetime = 500.f;
};

struct ParticleVertex {
    glm::vec3 pos;
    float size;
    glm::vec4 colorAlpha;
};

class ParticleSystem {
public:
    static constexpr size_t MAX_PARTICLES = 50000;

    void update(float dt, const PfxParams& params, const std::string& selectedName) {
        if (!selectedName.empty()) {
            if (params.ppsValue > 0.f) {
                spawnAccum += params.ppsValue * dt;
                while (spawnAccum >= 1.f && particles.size() < MAX_PARTICLES) {
                    spawnParticle(params);
                    spawnAccum -= 1.f;
                }
            } else if (particles.empty()) {
                for (int i = 0; i < 30 && particles.size() < MAX_PARTICLES; ++i) 
                    spawnParticle(params);
            }
        }

        for (size_t i = 0; i < particles.size();) {
            LiveParticle& lp = particles[i];
            lp.age += dt * 1000.f;
            if (lp.age >= lp.lifetime) {
                lp = particles.back();
                particles.pop_back();
                continue;
            }
            lp.vel += params.gravity * dt;
            lp.pos += lp.vel * dt;
            ++i;
        }

        buildRenderBuffer(params);
    }

    void clear() {
        particles.clear();
        renderBuf.clear();
        spawnAccum = 0.f;
    }

    static float estimateEffectRadius(const PfxParams& p) {
        float shapeExtent = std::max({p.shpDim.x, p.shpDim.y, p.shpDim.z}) * 0.5f;
        float speedMax     = p.velAvg + p.velVar;
        float lifeSec       = (p.lspAvg + p.lspVar) / 1000.f;
        float travel        = speedMax * lifeSec;
        float gravityDrop   = 0.5f * glm::length(p.gravity) * lifeSec * lifeSec;

        return std::max(shapeExtent + travel + gravityDrop, 50.f);
    }

    static void reframeCamera(const PfxParams& p) {
        float radius = estimateEffectRadius(p);
        float headRad = glm::radians(p.dirAngleHead);
        float elevRad = glm::radians(p.dirAngleElev);
        float cosElev = std::cos(elevRad);
        glm::vec3 baseDir(cosElev * std::sin(headRad), std::sin(elevRad), cosElev * std::cos(headRad));

        CameraController::g_cam.targetPos = p.shpOffset + baseDir * (radius * 0.2f);
        CameraController::g_cam.distance = std::clamp(radius * 2.5f, 20.f, 30000.f);
    }

    const std::vector<ParticleVertex>& getRenderBuffer() const { return renderBuf; }

private:
    std::vector<LiveParticle> particles;
    std::vector<ParticleVertex> renderBuf;
    float spawnAccum = 0.f;
    std::mt19937 rng{std::random_device{}()};

    float randRange(float lo, float hi) {
        if (hi < lo) std::swap(lo, hi);
        std::uniform_real_distribution<float> d(lo, hi);
        return d(rng);
    }

    glm::vec3 sampleEmitterPos(const PfxParams& p) {
        glm::vec3 local(0.f);
        if (p.shpType == "LINE") local.x = randRange(-p.shpDim.x * 0.5f, p.shpDim.x * 0.5f);
        else if (p.shpType == "BOX") {
            local.x = randRange(-p.shpDim.x * 0.5f, p.shpDim.x * 0.5f);
            local.y = randRange(-p.shpDim.y * 0.5f, p.shpDim.y * 0.5f);
            local.z = randRange(-p.shpDim.z * 0.5f, p.shpDim.z * 0.5f);
        } else if (p.shpType == "CIRCLE") {
            float r = p.shpDim.x;
            float ang = randRange(0.f, 6.2831853f);
            float rr = p.shpIsVolume ? r * std::sqrt(randRange(0.f, 1.f)) : r;
            local.x = rr * std::cos(ang);
            local.z = rr * std::sin(ang);
        } else if (p.shpType == "SPHERE") {
            float r = p.shpDim.x;
            float az = randRange(0.f, 6.2831853f);
            float el = randRange(-1.f, 1.f);
            float rr = p.shpIsVolume ? r * std::cbrt(randRange(0.f, 1.f)) : r;
            float sinEl = el;
            float cosEl = std::sqrt(std::max(0.f, 1.f - sinEl * sinEl));
            local = glm::vec3(rr * cosEl * std::cos(az), rr * sinEl, rr * cosEl * std::sin(az));
        }
        return local + p.shpOffset;
    }

    glm::vec3 sampleDirection(const PfxParams& p) {
        float head = (p.dirMode == "RAND") ? randRange(p.dirAngleHead - p.dirAngleHeadVar, p.dirAngleHead + p.dirAngleHeadVar) : p.dirAngleHead;
        float elev = (p.dirMode == "RAND") ? randRange(p.dirAngleElev - p.dirAngleElevVar, p.dirAngleElev + p.dirAngleElevVar) : p.dirAngleElev;
        float headRad = glm::radians(head);
        float elevRad = glm::radians(elev);
        float cosElev = std::cos(elevRad);
        return glm::vec3(cosElev * std::sin(headRad), std::sin(elevRad), cosElev * std::cos(headRad));
    }

    void spawnParticle(const PfxParams& p) {
        LiveParticle np;
        np.pos = sampleEmitterPos(p);
        glm::vec3 dir = sampleDirection(p);
        float speed = std::max(0.f, randRange(p.velAvg - p.velVar, p.velAvg + p.velVar));
        np.vel = dir * speed;
        np.age = 0.f;
        np.lifetime = std::max(10.f, randRange(p.lspAvg - p.lspVar, p.lspAvg + p.lspVar));
        particles.push_back(np);
    }

    void buildRenderBuffer(const PfxParams& currentParams) {
        renderBuf.clear();
        for (auto& lp : particles) {
            float t = std::clamp(lp.age / lp.lifetime, 0.f, 1.f);
            float sizeCm = glm::mix((currentParams.sizeStart.x + currentParams.sizeStart.y) * 0.5f,
                                     (currentParams.sizeStart.x + currentParams.sizeStart.y) * 0.5f * currentParams.sizeEndScale, t);
            glm::vec3 col = glm::mix(currentParams.colorStart, currentParams.colorEnd, t);
            float alpha = glm::mix(currentParams.alphaStart, currentParams.alphaEnd, t);

            ParticleVertex v;
            v.pos = lp.pos;
            v.size = sizeCm;
            v.colorAlpha = glm::vec4(col, alpha);
            renderBuf.push_back(v);
        }
    }
};