#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <cmath>

// ZenKit
#include <zenkit/World.hh>
#include <zenkit/Archive.hh>
#include <zenkit/Stream.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/VirtualObject.hh>

// OpenGL / GLFW / Epoxy
#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct LightData {
    std::string name;
    std::string preset;
    glm::vec3 originalZenPosition;
    glm::vec3 position;
    int type;           // 0 = POINT, 1 = SPOT
    glm::vec4 color;    // RGBA
    float range;        
    float coneAngle;
};

enum ObjectType { CUBE, PYRAMID };

struct SceneObject {
    ObjectType type;
    glm::vec3 position;
    glm::vec3 scale;
    float rotation;
    glm::vec3 color;
};

struct CameraState {
    float yaw = 0.0f;
    float pitch = 0.4f;
    float distance = 18.0f;
    glm::vec3 target = glm::vec3(0.0f, 0.5f, 0.0f);
    
    bool isDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
} gCamera;

void extractLights(const std::shared_ptr<zenkit::VirtualObject>& vob, std::vector<LightData>& lights) {
    if (vob->type == zenkit::VirtualObjectType::zCVobLight) {
        const auto& l = static_cast<const zenkit::VLight&>(*vob);
        
        LightData data;
        data.name = l.vob_name.empty() ? "zCVobLight" : l.vob_name;
        data.preset = l.preset;
        
        data.originalZenPosition = glm::vec3(l.position.x / 100.0f, l.position.y / 100.0f, l.position.z / 100.0f);
        data.position = glm::vec3(0.0f, 3.0f, 0.0f);
        data.type = (l.light_type == zenkit::LightType::SPOT) ? 1 : 0;
        
        float r = l.color.r / 255.0f;
        float g = l.color.g / 255.0f;
        float b = l.color.b / 255.0f;
        if (r + g + b < 0.1f) { r = 1.0f; g = 0.9f; b = 0.7f; }

        data.color = glm::vec4(r, g, b, 1.0f);
        
        float parsedRange = l.range / 100.0f;
        data.range = (parsedRange < 5.0f) ? 15.0f : parsedRange;
        data.coneAngle = (l.cone_angle <= 0.0f) ? 45.0f : l.cone_angle;

        lights.push_back(data);
    }

    for (const auto& child : vob->children) {
        extractLights(child, lights);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    gCamera.distance -= (float)yoffset * 1.2f;
    if (gCamera.distance < 1.0f) gCamera.distance = 1.0f;
    if (gCamera.distance > 150.0f) gCamera.distance = 150.0f;
}

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 viewPos;
uniform vec3 objectColor;
uniform bool isUnlit;

uniform int lightType;
uniform vec3 lightPos;
uniform vec3 lightDir;
uniform vec4 lightColor;
uniform float lightRange;
uniform float coneAngle;
uniform float ambientIntensity; // Dynamiczne światło otoczenia

void main() {
    if (isUnlit) {
        FragColor = vec4(objectColor, 1.0);
        return;
    }

    vec3 norm = normalize(Normal);
    vec3 globalAmbient = vec3(ambientIntensity); 
    
    vec3 lightVector = lightPos - FragPos;
    float dist = length(lightVector);
    vec3 lightDirNorm = normalize(lightVector);

    // Wzór Direct3D 7 (Gothic)
    float att1 = 0.009;
    float attenuation = 1.0 / (att1 * dist + 0.0001);

    if (lightRange > 0.0) {
        float cutoff = smoothstep(lightRange, lightRange * 0.7, dist);
        attenuation *= cutoff;
    }

    float diff = max(dot(norm, lightDirNorm), 0.0);
    vec3 diffuse = diff * lightColor.rgb;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDirNorm, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular = 0.3 * spec * lightColor.rgb;

    if (lightType == 1) {
        float theta = dot(lightDirNorm, normalize(-lightDir));
        float cutoff = cos(radians(coneAngle));
        float outerCutoff = cos(radians(coneAngle + 10.0));
        float epsilon = cutoff - outerCutoff;
        float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
        
        diffuse *= intensity;
        specular *= intensity;
    }

    vec3 lighting = globalAmbient + (diffuse + specular) * attenuation;
    FragColor = vec4(lighting * objectColor, 1.0);
}
)";

void createCube(std::vector<float>& vertices) {
    float cube[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };
    vertices.assign(cube, cube + sizeof(cube) / sizeof(float));
}

void createPyramid(std::vector<float>& vertices) {
    float n1 = 0.894427f;
    float n2 = 0.447214f;

    float pyramid[] = {
        -0.5f, 0.0f, -0.5f,  0.0f, -1.0f, 0.0f,
         0.5f, 0.0f, -0.5f,  0.0f, -1.0f, 0.0f,
         0.5f, 0.0f,  0.5f,  0.0f, -1.0f, 0.0f,
         0.5f, 0.0f,  0.5f,  0.0f, -1.0f, 0.0f,
        -0.5f, 0.0f,  0.5f,  0.0f, -1.0f, 0.0f,
        -0.5f, 0.0f, -0.5f,  0.0f, -1.0f, 0.0f,

        -0.5f, 0.0f,  0.5f,  0.0f, n2, n1,
         0.5f, 0.0f,  0.5f,  0.0f, n2, n1,
         0.0f, 1.0f,  0.0f,  0.0f, n2, n1,

         0.5f, 0.0f,  0.5f,  n1, n2, 0.0f,
         0.5f, 0.0f, -0.5f,  n1, n2, 0.0f,
         0.0f, 1.0f,  0.0f,  n1, n2, 0.0f,

         0.5f, 0.0f, -0.5f,  0.0f, n2, -n1,
        -0.5f, 0.0f, -0.5f,  0.0f, n2, -n1,
         0.0f, 1.0f,  0.0f,  0.0f, n2, -n1,

        -0.5f, 0.0f, -0.5f, -n1, n2, 0.0f,
        -0.5f, 0.0f,  0.5f, -n1, n2, 0.0f,
         0.0f, 1.0f,  0.0f, -n1, n2, 0.0f
    };
    vertices.assign(pyramid, pyramid + sizeof(pyramid) / sizeof(float));
}

void createPlane(std::vector<float>& vertices) {
    float plane[] = {
         20.0f, 0.0f,  20.0f,  0.0f, 1.0f, 0.0f,
        -20.0f, 0.0f,  20.0f,  0.0f, 1.0f, 0.0f,
        -20.0f, 0.0f, -20.0f,  0.0f, 1.0f, 0.0f,

         20.0f, 0.0f,  20.0f,  0.0f, 1.0f, 0.0f,
        -20.0f, 0.0f, -20.0f,  0.0f, 1.0f, 0.0f,
         20.0f, 0.0f, -20.0f,  0.0f, 1.0f, 0.0f
    };
    vertices.assign(plane, plane + sizeof(plane) / sizeof(float));
}

std::vector<SceneObject> generateSceneObjects() {
    std::vector<SceneObject> objects;

    objects.push_back({CUBE, glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(1.2f), 0.0f, glm::vec3(0.85f, 0.25f, 0.2f)});
    objects.push_back({PYRAMID, glm::vec3(3.0f, 0.0f, 0.0f), glm::vec3(1.5f, 2.0f, 1.5f), 0.0f, glm::vec3(0.2f, 0.7f, 0.3f)});
    objects.push_back({PYRAMID, glm::vec3(-3.0f, 0.0f, 0.0f), glm::vec3(1.5f, 2.5f, 1.5f), 45.0f, glm::vec3(0.2f, 0.4f, 0.8f)});
    objects.push_back({CUBE, glm::vec3(0.0f, 0.75f, 3.0f), glm::vec3(1.5f), 25.0f, glm::vec3(0.8f, 0.6f, 0.2f)});
    objects.push_back({CUBE, glm::vec3(0.0f, 0.5f, -3.0f), glm::vec3(1.0f), 15.0f, glm::vec3(0.7f, 0.2f, 0.7f)});

    float edgeOffset = 18.0f;
    int itemsPerSide = 8;
    float step = (edgeOffset * 2.0f) / itemsPerSide;

    for (int i = 0; i <= itemsPerSide; ++i) {
        float pos = -edgeOffset + i * step;
        
        objects.push_back({(i % 2 == 0) ? PYRAMID : CUBE, glm::vec3(pos, 0.0f, -edgeOffset), glm::vec3(1.5f, (i%3+1)*1.2f, 1.5f), i * 30.0f, glm::vec3(0.4f, 0.5f, 0.6f)});
        objects.push_back({(i % 2 != 0) ? PYRAMID : CUBE, glm::vec3(pos, 0.0f, edgeOffset), glm::vec3(1.5f, (i%2+1)*1.5f, 1.5f), i * 15.0f, glm::vec3(0.5f, 0.4f, 0.5f)});
        
        if (i > 0 && i < itemsPerSide) {
            objects.push_back({(i % 2 != 0) ? PYRAMID : CUBE, glm::vec3(-edgeOffset, 0.0f, pos), glm::vec3(1.5f, (i%3+1)*1.0f, 1.5f), i * 45.0f, glm::vec3(0.6f, 0.5f, 0.3f)});
            objects.push_back({(i % 2 == 0) ? PYRAMID : CUBE, glm::vec3(edgeOffset, 0.0f, pos), glm::vec3(1.5f, (i%2+1)*1.8f, 1.5f), i * 20.0f, glm::vec3(0.3f, 0.6f, 0.5f)});
        }
    }

    return objects;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uzycie: " << argv[0] << " sciezka/do/plik.ZEN\n";
        return 1;
    }

    std::vector<LightData> loadedLights;
    try {
        zenkit::World world;
        auto reader = zenkit::Read::from(std::filesystem::path(argv[1]));
        world.load(reader.get());

        for (const auto& vob : world.world_vobs) {
            extractLights(vob, loadedLights);
        }
        std::cout << "Wczytano " << loadedLights.size() << " swiatel z zCVobLight.\n";
    } catch (const std::exception& e) {
        std::cerr << "Blad ZenKit: " << e.what() << "\n";
        return 1;
    }

    if (loadedLights.empty()) {
        loadedLights.push_back({"Default_Light", "Preset", glm::vec3(0.0f), glm::vec3(0.0f, 3.0f, 0.0f), 0, glm::vec4(1.0f, 0.9f, 0.6f, 1.0f), 15.0f, 45.0f});
    }

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "ZenKit Light Tester", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetScrollCallback(window, scroll_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glEnable(GL_DEPTH_TEST);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    std::vector<float> cubeVerts, pyramidVerts, planeVerts;
    createCube(cubeVerts);
    createPyramid(pyramidVerts);
    createPlane(planeVerts);

    GLuint cubeVAO, cubeVBO, pyramidVAO, pyramidVBO, planeVAO, planeVBO;
    
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, cubeVerts.size() * sizeof(float), cubeVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &pyramidVAO);
    glGenBuffers(1, &pyramidVBO);
    glBindVertexArray(pyramidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pyramidVBO);
    glBufferData(GL_ARRAY_BUFFER, pyramidVerts.size() * sizeof(float), pyramidVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, planeVerts.size() * sizeof(float), planeVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    auto sceneObjects = generateSceneObjects();
    int selectedLightIdx = 0;
    float globalAmbientIntensity = 0.12f; // Domyślna wartość Ambient

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        if (!io.WantCaptureMouse) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                double mouseX, mouseY;
                glfwGetCursorPos(window, &mouseX, &mouseY);

                if (!gCamera.isDragging) {
                    gCamera.isDragging = true;
                    gCamera.lastMouseX = mouseX;
                    gCamera.lastMouseY = mouseY;
                } else {
                    float dx = (float)(mouseX - gCamera.lastMouseX);
                    float dy = (float)(mouseY - gCamera.lastMouseY);

                    gCamera.yaw += dx * 0.005f;
                    gCamera.pitch += dy * 0.005f;

                    if (gCamera.pitch > 1.55f) gCamera.pitch = 1.55f;
                    if (gCamera.pitch < -1.55f) gCamera.pitch = -1.55f;

                    gCamera.lastMouseX = mouseX;
                    gCamera.lastMouseY = mouseY;
                }
            } else {
                gCamera.isDragging = false;
            }
        } else {
            gCamera.isDragging = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(380, 690), ImGuiCond_Once);
        ImGui::Begin("ZenKit - Edytor Swiatel ZEN", NULL);

        ImGui::Text("Lista Swiatel z ZEN (%zu):", loadedLights.size());
        ImGui::Separator();

        ImGui::BeginChild("LightList", ImVec2(0, 180), true);
        for (int i = 0; i < (int)loadedLights.size(); ++i) {
            std::string label = "[" + std::to_string(i) + "] " + loadedLights[i].name;
            if (!loadedLights[i].preset.empty()) {
                label += " (" + loadedLights[i].preset + ")";
            }

            if (ImGui::Selectable(label.c_str(), selectedLightIdx == i)) {
                selectedLightIdx = i;
            }
        }
        ImGui::EndChild();

        LightData& currentLight = loadedLights[selectedLightIdx];
        ImGui::Separator();
        
        // NOWOŚĆ: Suwak Światła Otoczenia (Global Ambient)
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Oświetlenie Globalne Scene:");
        ImGui::SliderFloat("Ambient (Otoczenie)", &globalAmbientIntensity, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Oryginalna pozycja w pliku ZEN:");
        ImGui::Text("X: %.2f m  Y: %.2f m  Z: %.2f m", 
            currentLight.originalZenPosition.x, 
            currentLight.originalZenPosition.y, 
            currentLight.originalZenPosition.z);
        
        ImGui::Separator();
        ImGui::Text("Modyfikacja pozycji w scenie 3D:");
        ImGui::InputFloat3("Pozycja (Wpisz)", glm::value_ptr(currentLight.position), "%.2f");
        ImGui::DragFloat3("Pozycja (Przeciągnij)", glm::value_ptr(currentLight.position), 0.1f);
        
        if (ImGui::Button("Resetuj pozycje (0, 3, 0)")) {
            currentLight.position = glm::vec3(0.0f, 3.0f, 0.0f);
        }

        ImGui::Separator();
        ImGui::Text("Parametry Swiatla:");
        
        const char* types[] = { "POINT", "SPOT" };
        ImGui::Combo("Typ Światła", &currentLight.type, types, IM_ARRAYSIZE(types));

        ImGui::ColorEdit4("Kolor (RGBA)", glm::value_ptr(currentLight.color));
        ImGui::SliderFloat("Zasięg (Range)", &currentLight.range, 0.5f, 100.0f);
        
        if (currentLight.type == 1) {
            ImGui::SliderFloat("Kąt Stożka", &currentLight.coneAngle, 1.0f, 89.0f);
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Kamera:");
        ImGui::Text("LPM + Przeciągnięcie = Obrót");
        ImGui::Text("Scroll = Zoom (Odległość: %.1fm)", gCamera.distance);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Wciśnij [ESC] lub [Q] aby wyjść.");
        
        ImGui::End();

        // RENDER OPENGL
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        float camX = gCamera.target.x + gCamera.distance * cos(gCamera.pitch) * sin(gCamera.yaw);
        float camY = gCamera.target.y + gCamera.distance * sin(gCamera.pitch);
        float camZ = gCamera.target.z + gCamera.distance * cos(gCamera.pitch) * cos(gCamera.yaw);
        glm::vec3 cameraPos = glm::vec3(camX, camY, camZ);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)display_w / (float)display_h, 0.1f, 300.0f);
        glm::mat4 view = glm::lookAt(cameraPos, gCamera.target, glm::vec3(0.0f, 1.0f, 0.0f));
        
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glUniform1i(glGetUniformLocation(shaderProgram, "isUnlit"), GL_FALSE);
        glUniform1i(glGetUniformLocation(shaderProgram, "lightType"), currentLight.type);
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(currentLight.position));
        glUniform3f(glGetUniformLocation(shaderProgram, "lightDir"), 0.0f, -1.0f, 0.0f);
        glUniform4fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(currentLight.color));
        glUniform1f(glGetUniformLocation(shaderProgram, "lightRange"), currentLight.range);
        glUniform1f(glGetUniformLocation(shaderProgram, "coneAngle"), currentLight.coneAngle);
        glUniform1f(glGetUniformLocation(shaderProgram, "ambientIntensity"), globalAmbientIntensity); // Przekazanie wartości Ambient
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

        // Podłoga (Plane)
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.45f, 0.45f, 0.5f);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Renderowanie obiektów sceny
        for (const auto& obj : sceneObjects) {
            model = glm::translate(glm::mat4(1.0f), obj.position);
            model = glm::rotate(model, glm::radians(obj.rotation), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, obj.scale);

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(obj.color));

            if (obj.type == CUBE) {
                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            } else if (obj.type == PYRAMID) {
                glBindVertexArray(pyramidVAO);
                glDrawArrays(GL_TRIANGLES, 0, 18);
            }
        }

        // Świecąca kostka światła
        glUniform1i(glGetUniformLocation(shaderProgram, "isUnlit"), GL_TRUE);
        model = glm::translate(glm::mat4(1.0f), currentLight.position);
        model = glm::scale(model, glm::vec3(0.3f));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), currentLight.color.r, currentLight.color.g, currentLight.color.b);
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ImGui Render
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &pyramidVAO);
    glDeleteBuffers(1, &pyramidVBO);
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}