#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <variant>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Camera.hpp"
#include "ParticleSystem.hpp"
#include "ParticleLibrary.hpp"
#include "ShaderUtils.hpp"
#include "GuiManager.hpp"

int main(int argc, char** argv) {
    auto datPaths = ParticleLibrary::discoverDatFiles(argc, argv);
    if (datPaths.empty()) {
        fprintf(stderr, "Brak sciezki do PARTICLEFX.DAT lub VISUALFX.DAT.\n");
        return 1;
    }

    ParticleLibrary lib;
    for (const auto& path : datPaths) {
        if (lib.loadFile(path)) printf("Zaladowano skrypt: %s\n", path.c_str());
    }

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(1400, 800, "pfxview - Gothic Particle Inspector", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glfwSetKeyCallback(win, CameraController::keyCallback);
    glfwSetCursorPosCallback(win, CameraController::cursorCallback);
    glfwSetMouseButtonCallback(win, CameraController::mouseButtonCallback);
    glfwSetScrollCallback(win, CameraController::scrollCallback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    GLuint pvs = ShaderUtils::compileShader(GL_VERTEX_SHADER, ShaderUtils::VERT_SRC);
    GLuint pfs = ShaderUtils::compileShader(GL_FRAGMENT_SHADER, ShaderUtils::FRAG_SRC);
    GLuint particleProg = ShaderUtils::linkProgram(pvs, pfs);
    glDeleteShader(pvs); glDeleteShader(pfs);

    GLuint fvs = ShaderUtils::compileShader(GL_VERTEX_SHADER, ShaderUtils::FLOOR_VERT_SRC);
    GLuint ffs = ShaderUtils::compileShader(GL_FRAGMENT_SHADER, ShaderUtils::FLOOR_FRAG_SRC);
    GLuint floorProg = ShaderUtils::linkProgram(fvs, ffs);
    glDeleteShader(fvs); glDeleteShader(ffs);

    float R = 2000.f;
    float floorVerts[] = { -R,0,-R, R,0,-R, R,0,R, -R,0,-R, R,0,R, -R,0,R };
    GLuint floorVao, floorVbo;
    glGenVertexArrays(1, &floorVao);
    glGenBuffers(1, &floorVbo);
    glBindVertexArray(floorVao);
    glBindBuffer(GL_ARRAY_BUFFER, floorVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVerts), floorVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    GLuint pVao, pVbo;
    glGenVertexArrays(1, &pVao);
    glGenBuffers(1, &pVbo);
    glBindVertexArray(pVao);
    glBindBuffer(GL_ARRAY_BUFFER, pVbo);
    glBufferData(GL_ARRAY_BUFFER, ParticleSystem::MAX_PARTICLES * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, size));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, colorAlpha));
    glEnableVertexAttribArray(2);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    GuiManager::initFonts();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ParticleSystem particleSys;
    
    // Rozdzielony stan efektów (variant)
    ActiveEffectParams activeParams;
    
    Texture2D currentTexture;
    std::string selectedName;
    bool autoZoom = true;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = std::min(float(now - lastTime), 0.05f);
        lastTime = now;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Rysowanie UI – modyfikuje activeParams
        GuiManager::drawUI(lib, selectedName, activeParams, particleSys, autoZoom, currentTexture, datPaths);

        CameraController::updateMovement(dt);

        if (CameraController::g_resetRequested) {
            particleSys.clear();
            CameraController::g_resetRequested = false;
        }

        // Aktualizuj system cząsteczek tylko wtedy, gdy wybrany efekt to PFX
        auto* pfxParams = std::get_if<PfxParams>(&activeParams);
        if (pfxParams) {
            particleSys.update(dt, *pfxParams, selectedName);
        } else {
            particleSys.clear(); // Czyści starą symulację, jeśli przełączyliśmy się na VFX
        }

        const auto& renderBuf = particleSys.getRenderBuffer();
        if (pfxParams && !renderBuf.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, pVbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(renderBuf.size() * sizeof(ParticleVertex)), renderBuf.data());
        }

        int fbw, fbh;
        glfwGetFramebufferSize(win, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.02f, 0.02f, 0.03f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(60.f), float(fbw) / float(fbh), 1.f, 100000.f);
        glm::mat4 view = CameraController::g_cam.view();

        // Rysowanie podłogi
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glUseProgram(floorProg);
        glUniformMatrix4fv(glGetUniformLocation(floorProg, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(floorProg, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        glBindVertexArray(floorVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Rysowanie cząsteczek (tylko gdy aktywne jest PFX)
        if (pfxParams && !renderBuf.empty()) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glUseProgram(particleProg);
            glUniformMatrix4fv(glGetUniformLocation(particleProg, "uView"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(particleProg, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform1f(glGetUniformLocation(particleProg, "uViewportHeight"), float(fbh));

            GLint useTexLoc = glGetUniformLocation(particleProg, "uUseTexture");
            GLint texLoc    = glGetUniformLocation(particleProg, "uTexture");

            if (currentTexture.valid && currentTexture.id != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, currentTexture.id);
                glUniform1i(texLoc, 0);
                glUniform1i(useTexLoc, 1);
            } else {
                glUniform1i(useTexLoc, 0);
            }

            glBindVertexArray(pVao);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(renderBuf.size()));
        }

        glDepthMask(GL_TRUE);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    currentTexture.free();
    glDeleteVertexArrays(1, &pVao); glDeleteBuffers(1, &pVbo);
    glDeleteVertexArrays(1, &floorVao); glDeleteBuffers(1, &floorVbo);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();

    return 0;
}