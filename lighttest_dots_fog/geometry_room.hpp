#pragma once

#include <vector>
#include <epoxy/gl.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "zen_loader.hpp"

static GLuint makeFullscreenQuad() {
  float verts[] = {
    -1.f,-1.f,  1.f,-1.f,  1.f,1.f,
    -1.f,-1.f,  1.f,1.f,  -1.f,1.f,
  };
  GLuint vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);
  return vao;
}


static void addQuad(std::vector<Vertex>& out,
                    glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                    glm::vec3 n) 
{
    out.push_back({a, n, {0.f, 0.f}}); 
    out.push_back({b, n, {1.f, 0.f}}); 
    out.push_back({c, n, {1.f, 1.f}}); 
    out.push_back({a, n, {0.f, 0.f}}); 
    out.push_back({c, n, {1.f, 1.f}}); 
    out.push_back({d, n, {0.f, 1.f}});
}

static void addTriangle(std::vector<Vertex>& out,
                        glm::vec3 a, glm::vec3 b, glm::vec3 c,
                        glm::vec3 normal)
{
    out.push_back({a, normal, {0.f, 0.f}});
    out.push_back({b, normal, {1.f, 0.f}});
    out.push_back({c, normal, {0.5f, 1.f}});
}

static void addBox(std::vector<Vertex>& out, glm::vec3 c, glm::vec3 half) {
  glm::vec3 p[8] = {
    c+glm::vec3(-half.x,-half.y,-half.z), c+glm::vec3( half.x,-half.y,-half.z),
    c+glm::vec3( half.x, half.y,-half.z), c+glm::vec3(-half.x, half.y,-half.z),
    c+glm::vec3(-half.x,-half.y, half.z), c+glm::vec3( half.x,-half.y, half.z),
    c+glm::vec3( half.x, half.y, half.z), c+glm::vec3(-half.x, half.y, half.z),
    };
  addQuad(out, p[0],p[1],p[2],p[3], { 0, 0,-1}); // przod
  addQuad(out, p[5],p[4],p[7],p[6], { 0, 0, 1}); // tyl
  addQuad(out, p[4],p[0],p[3],p[7], {-1, 0, 0}); // lewo
  addQuad(out, p[1],p[5],p[6],p[2], { 1, 0, 0}); // prawo
  addQuad(out, p[3],p[2],p[6],p[7], { 0, 1, 0}); // gora
  addQuad(out, p[4],p[5],p[1],p[0], { 0,-1, 0}); // dol
  }



static void addTriangle(
    std::vector<Vertex>& out,
    glm::vec3 center,
    float width,
    float height,
    glm::vec3 normal)
{
    glm::vec3 a = center + glm::vec3(-width * 0.5f, -height * 0.5f, 0.f);
    glm::vec3 b = center + glm::vec3( width * 0.5f, -height * 0.5f, 0.f);
    glm::vec3 c = center + glm::vec3(0.f, height * 0.5f, 0.f);

    out.push_back({a, normal});
    out.push_back({b, normal});
    out.push_back({c, normal});
}


// pokoj: podloga + 4 sciany, skala w cm (jak w Gothicu) - promien ~1500,
// wysokosc scian 400, zeby bylo miejsce na testowanie duzych Range (AURA=3000
// wystaje poza pokoj celowo - i o to chodzi, zeby zobaczyc peine gasniecie)
static std::vector<Vertex> buildRoom()
{
    std::vector<Vertex> v;

    float R = 1500.f;
    float H = 400.f;
    float T = 20.f;

    // --------------------------------------------------
    // PODŁOGA
    // --------------------------------------------------
    addQuad(
        v,
        {-R, 0.f, -R},
        { R, 0.f, -R},
        { R, 0.f,  R},
        {-R, 0.f,  R},
        {0.f, 1.f, 0.f}
    );

    // --------------------------------------------------
    // SUFIT
    // --------------------------------------------------
    float ceilingR = R / 5.f;

    addQuad(
        v,
        {-ceilingR, H, -ceilingR},
        { ceilingR, H, -ceilingR},
        { ceilingR, H,  ceilingR},
        {-ceilingR, H,  ceilingR},
        {0.f, -1.f, 0.f}
    );

    // --------------------------------------------------
    // JEDNA ŚCIANA ZA ŚWIATŁEM
    // Z = -500
    // --------------------------------------------------
    addBox(
        v,
        {0.f, H * 0.5f, -500.f},
        {600.f, H * 0.5f, T}
    );

    // --------------------------------------------------
    // TRÓJKĄT JESZCZE DALEJ
    // Z = -700
    // --------------------------------------------------
    addTriangle(
        v,
        {-500.f, 0.f, -700.f},
        { 500.f, 0.f, -700.f},
        {   0.f, H,   -700.f},
        {0.f, 0.f, 1.f}
    );

    // ==================================================
    // OBIEKTY DALEKO OD ŚWIATŁA - ok. 10x dalej
    // ==================================================

    // Duża ściana w tle
    addBox(
        v,
        {0.f, 200.f, -5000.f},
        {800.f, 200.f, 20.f}
    );

    // Wąska wysoka kolumna po lewej
    addBox(
        v,
        {-1000.f, 300.f, -4500.f},
        {100.f, 300.f, 100.f}
    );

    // Wąska kolumna po prawej
    addBox(
        v,
        {1000.f, 200.f, -5500.f},
        {150.f, 200.f, 150.f}
    );

    // Duży trójkąt jeszcze dalej
    addTriangle(
        v,
        {-500.f, 0.f, -5000.f},
        { 500.f, 0.f, -5000.f},
        {   0.f, 800.f, -5000.f},
        {0.f, 0.f, 1.f}
    );


    return v;
}