#include <epoxy/gl.h> // Wystarczy tylko ten nagłówek zamiast GLAD/GLEW!
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>

// --- PARAMS (Symulacja zparsowanego pliku ParticleFX.d) ---
struct PFXConfig {
    float ppsValue = 40.0f;           // Liczba cząsteczek
    float shpRadius = 0.3f;           // SPHERE radius (rozmiar wokół mikstury)
    float particleSize = 0.08f;       // visSizeStart
    float flySpeed = 0.15f;           // velAvg
    float particleLifetime = 1.5f;    // Czas życia pojedynczej gwiazdki (sekundy)
} pfxConfig;

// --- DANE CZĄSTECZKI (CPU) ---
struct Particle {
    glm::vec3 basePos;
    glm::vec3 pos;      
    glm::vec3 dir;      
    float life = 0.0f;  
    float speed = 1.0f;
};

// --- SHADERY GLSL ---
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;         // Quad (-0.5 do 0.5)
layout (location = 1) in vec2 aTexCoords;   // UV (0 do 1)

// Dane z Instanced Buffer (per cząsteczka)
layout (location = 2) in vec3 aParticlePos; 
layout (location = 3) in float aLifetime;   

uniform mat4 uProjection;
uniform mat4 uView;
uniform float uSize;

out vec2 TexCoords;
out float Lifetime;

void main()
{
    TexCoords = aTexCoords;
    Lifetime = aLifetime;

    // --- CYRKUŁOWY BILLBOARDING ---
    vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 cameraUp    = vec3(uView[0][1], uView[1][1], uView[2][1]);

    vec3 worldPos = aParticlePos 
                  + cameraRight * aPos.x * uSize
                  + cameraUp    * aPos.y * uSize;

    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoords;
in float Lifetime;

uniform sampler2D uTexture;
out vec4 FragColor;

void main()
{
    vec4 texColor = texture(uTexture, TexCoords);

    // Miganie/Wygasanie (Alpha fade: 0.0 -> 1.0 -> 0.0)
    float alphaFade = sin(Lifetime * 3.14159265);

    // Czerwono-pomarańczowy odcień (jak w miksturze zdrowia w Gothicu)
    vec3 glowColor = vec3(1.0, 0.3, 0.2); 
    FragColor = vec4(texColor.rgb * glowColor, texColor.a * alphaFade);
}
)";

// --- GENEROWANIE PROCEDURALNEJ TEKSTURY GWIAZDKI ---
GLuint createStarTexture() {
    const int width = 64, height = 64;
    std::vector<unsigned char> data(width * height * 4);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = (x / (float)width) * 2.0f - 1.0f;
            float ny = (y / (float)height) * 2.0f - 1.0f;
            float dist = std::sqrt(nx * nx + ny * ny);

            float intensity = std::pow(std::max(0.0f, 1.0f - dist), 2.5f);
            
            int index = (y * width + x) * 4;
            data[index + 0] = 255; // R
            data[index + 1] = 255; // G
            data[index + 2] = 255; // B
            data[index + 3] = static_cast<unsigned char>(intensity * 255); // Alpha
        }
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    return textureID;
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

int main() {
    // 1. Inicjalizacja GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Gothic PFX Potion - Epoxy + OpenGL", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

// W EPOXY NIE MUSISZ WYWOŁYWAĆ DEDYKOWANEJ FUNKCJI ŁADOWANIA! 
    // Funkcje GL są od razu gotowe do użycia po utagowaniu kontekstu przez GLFW.

    // 2. Kompilacja Shaderów (Używa natywnych opcji Epoxy)
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    // 3. Geometria Quada dla pojedynczej cząsteczki
    float quadVertices[] = {
        -0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,

        -0.5f,  0.5f,  0.0f, 1.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f
    };

    GLuint VAO, VBO, instanceVBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // 4. Inicjalizacja cząsteczek
    int maxParticles = (int)pfxConfig.ppsValue;
    std::vector<Particle> particles(maxParticles);
    
    auto respawnParticle = [](Particle& p) {
        float theta = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
        float phi   = ((float)rand() / RAND_MAX) * 3.14159f;
        float r     = ((float)rand() / RAND_MAX) * pfxConfig.shpRadius;

        p.basePos = glm::vec3(
            r * std::sin(phi) * std::cos(theta),
            r * std::cos(phi),
            r * std::sin(phi) * std::sin(theta)
        );
        p.pos = p.basePos;
        p.dir = glm::normalize(p.basePos + glm::vec3(0.001f));
        p.life = ((float)rand() / RAND_MAX);
        p.speed = pfxConfig.flySpeed * (0.8f + ((float)rand() / RAND_MAX) * 0.4f);
    };

    for (auto& p : particles) respawnParticle(p);

    std::vector<float> instanceData(maxParticles * 4);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(float), nullptr, GL_STREAM_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    GLuint starTexture = createStarTexture();
    float lastFrame = 0.0f;

    // 5. PĘTLA RENDEROWANIA
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        // Fizyka cząsteczek na CPU
        for (int i = 0; i < maxParticles; ++i) {
            Particle& p = particles[i];
            p.life += deltaTime / pfxConfig.particleLifetime;
            
            p.pos += (p.dir * 0.2f + glm::vec3(0.0f, 0.5f, 0.0f)) * p.speed * deltaTime;

            if (p.life >= 1.0f) {
                respawnParticle(p);
                p.life = 0.0f;
            }

            instanceData[i * 4 + 0] = p.pos.x;
            instanceData[i * 4 + 1] = p.pos.y;
            instanceData[i * 4 + 2] = p.pos.z;
            instanceData[i * 4 + 3] = p.life;
        }

        // Przesyłanie zaktualizowanych instancji
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceData.size() * sizeof(float), instanceData.data());

        // Renderowanie
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        // Blending z użyciem Epoxy API
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); 
        glDepthMask(GL_FALSE); 

        glUseProgram(shaderProgram);

        // Macierze widoku i projekcji
        float camX = std::sin(currentFrame * 0.5f) * 2.0f;
        float camZ = std::cos(currentFrame * 0.5f) * 2.0f;
        glm::mat4 view = glm::lookAt(glm::vec3(camX, 0.5f, camZ), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1024.0f / 768.0f, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform1f(glGetUniformLocation(shaderProgram, "uSize"), pfxConfig.particleSize);

        glBindTexture(GL_TEXTURE_2D, starTexture);
        glBindVertexArray(VAO);
        
        // Epoxy wywołuje glDrawArraysInstanced bez potrzeby pobierania funkcji przez dlsym
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, maxParticles);

        glDepthMask(GL_TRUE); 

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &instanceVBO);
    glfwTerminate();
    return 0;
}