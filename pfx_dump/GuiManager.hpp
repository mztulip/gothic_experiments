#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "ParticleLibrary.hpp"
#include "ParticleSystem.hpp"
#include "PfxParams.hpp"
#include "texture_loader.h"

class GuiManager {
public:
    static void initFonts() {
        ImGuiIO& io = ImGui::GetIO();
        static const ImWchar polish_ranges[] = { 0x0020, 0x00FF, 0x0100, 0x017F, 0 };
        const char* fontPaths[] = {
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/gnu-free/FreeSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf"
        };

        ImFont* font = nullptr;
        for (const char* path : fontPaths) {
            if (std::filesystem::exists(path)) {
                font = io.Fonts->AddFontFromFileTTF(path, 16.0f, nullptr, polish_ranges);
                if (font) break;
            }
        }
        if (!font) io.Fonts->AddFontDefault();
    }

    static void loadSelectedEffect(const std::string& name, const ParticleLibrary& lib, 
                                   PfxParams& currentParams, ParticleSystem& sys, 
                                   bool autoZoom, Texture2D& currentTexture, 
                                   const std::vector<std::string>& datPaths) 
    {
        if (!lib.getParams(name, currentParams)) return;

        sys.clear();
        if (autoZoom) ParticleSystem::reframeCamera(currentParams);

        currentTexture.free();
        currentParams.loadedTexturePath.clear();

        std::string texToLoad = currentParams.visName;
        if (containsIgnoreCase(texToLoad, ".3ds") || containsIgnoreCase(texToLoad, ".mms")) {
            texToLoad = "";
        }

        if (!texToLoad.empty()) {
            std::string gothicDir = ParticleLibrary::resolveGothicDir(datPaths);
            std::string fullPath = TextureLoader::resolveGothicTexturePath(texToLoad, gothicDir);

            if (!fullPath.empty()) {
                currentTexture = TextureLoader::loadFromFile(fullPath, false);
                if (currentTexture.valid) {
                    currentParams.loadedTexturePath = fullPath;
                }
            }
        }
    }

    static void drawUI(const ParticleLibrary& lib, std::string& selectedName, 
                       PfxParams& currentParams, ParticleSystem& sys, 
                       bool& autoZoom, Texture2D& currentTexture, 
                       const std::vector<std::string>& datPaths) 
    {
        static char searchBuffer[128] = "";

        std::string searchFilter(searchBuffer);
        
        // Podział efektów na PFX oraz VFX z uwzględnieniem filtra wyszukiwania
        std::vector<std::string> filteredPfx;
        std::vector<std::string> filteredVfx;

        for (const auto& name : lib.names()) {
            if (containsIgnoreCase(name, searchFilter)) {
                std::string origin = lib.getOriginFile(name);
                if (containsIgnoreCase(origin, "VISUALFX")) {
                    filteredVfx.push_back(name);
                } else {
                    filteredPfx.push_back(name);
                }
            }
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340, 800), ImGuiCond_FirstUseEver);
        ImGui::Begin("Efekty (DAT)");
        ImGui::Text("Suma efektow: %zu", lib.names().size());
        ImGui::Checkbox("Auto Zoom", &autoZoom);
        ImGui::SameLine();
        if (!selectedName.empty() && ImGui::Button("Dopasuj")) ParticleSystem::reframeCamera(currentParams);

        ImGui::InputText("##szukaj", searchBuffer, IM_ARRAYSIZE(searchBuffer));
        ImGui::SameLine();
        if (ImGui::Button("X")) searchBuffer[0] = '\0';

        ImGui::Separator();

        // System tabów podzielony na PFX oraz VFX
        if (ImGui::BeginTabBar("EffectTypeTabs")) {
            
            // --- TAB 1: PARTICLE EFFECTS ---
            if (ImGui::BeginTabItem("ParticleFX")) {
                handleNavKeys(filteredPfx, selectedName, lib, currentParams, sys, autoZoom, currentTexture, datPaths);
                
                ImGui::BeginChild("lista_pfx");
                renderEffectList(filteredPfx, selectedName, lib, currentParams, sys, autoZoom, currentTexture, datPaths);
                ImGui::EndChild();
                
                ImGui::EndTabItem();
            }

            // --- TAB 2: VISUAL EFFECTS ---
            if (ImGui::BeginTabItem("VisualFX")) {
                handleNavKeys(filteredVfx, selectedName, lib, currentParams, sys, autoZoom, currentTexture, datPaths);
                
                ImGui::BeginChild("lista_vfx");
                renderEffectList(filteredVfx, selectedName, lib, currentParams, sys, autoZoom, currentTexture, datPaths);
                ImGui::EndChild();
                
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        /* Panel Szczegółów */
        ImGui::SetNextWindowPos(ImVec2(350, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
        ImGui::Begin("Inspector Szczegolow Efektu");
        if (!selectedName.empty()) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Nazwa Instancji:"); ImGui::SameLine(); ImGui::Text("%s", selectedName.c_str());
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Plik Skryptu (.DAT):"); ImGui::SameLine(); ImGui::Text("%s", currentParams.originDatFile.c_str());
            ImGui::Separator();
            ImGui::TextUnformatted("--- TEKSTURA ---");
            ImGui::Text("Deklarowana w Daedalus: %s", currentParams.visName.empty() ? "(Brak)" : currentParams.visName.c_str());

            if (!currentParams.loadedTexturePath.empty()) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Sciezka pliku:");
                ImGui::TextWrapped("%s", currentParams.loadedTexturePath.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Stan: Nieodnaleziona w katalogach/_compiled/VDF");
            }

            if (currentTexture.valid && currentTexture.id != 0) {
                ImGui::Text("Rozmiar GL: %dx%d px", currentTexture.width, currentTexture.height);
                ImGui::Image((void*)(intptr_t)currentTexture.id, ImVec2(80, 80));
            }

            ImGui::Separator();
            ImGui::TextUnformatted("--- PARAMETRY EMISJI ---");
            ImGui::Text("Szybkosć Emisji (ppsValue): %.2f", currentParams.ppsValue);
            ImGui::Text("Ksztalt Emitera (shpType): %s", currentParams.shpType.c_str());
            ImGui::Text("Wymiary Emitera: [%.1f, %.1f, %.1f]", currentParams.shpDim.x, currentParams.shpDim.y, currentParams.shpDim.z);
            ImGui::Text("Predkosc Cząsteczek: %.1f (+-%.1f)", currentParams.velAvg, currentParams.velVar);
            ImGui::Text("Czas Zycia: %.0f ms (+-%.0f ms)", currentParams.lspAvg, currentParams.lspVar);
            ImGui::Text("Grawitacja: [%.1f, %.1f, %.1f]", currentParams.gravity.x, currentParams.gravity.y, currentParams.gravity.z);
            ImGui::Text("Skala Rozmiaru (Start->End): [%.1f,%.1f] -> x%.2f", currentParams.sizeStart.x, currentParams.sizeStart.y, currentParams.sizeEndScale);
        } else {
            ImGui::TextDisabled("Wybierz efekt z listy, aby wyswietlic szczegoly.");
        }
        ImGui::End();
    }

private:
    static void handleNavKeys(const std::vector<std::string>& list, std::string& selectedName, 
                              const ParticleLibrary& lib, PfxParams& currentParams, 
                              ParticleSystem& sys, bool autoZoom, Texture2D& currentTexture, 
                              const std::vector<std::string>& datPaths) 
    {
        if (ImGui::GetIO().WantTextInput || list.empty()) return;

        int currentIndex = -1;
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i] == selectedName) { currentIndex = static_cast<int>(i); break; }
        }

        int newIndex = currentIndex;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
            newIndex = (currentIndex <= 0) ? static_cast<int>(list.size()) - 1 : currentIndex - 1;
        else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
            newIndex = (currentIndex < 0 || currentIndex >= static_cast<int>(list.size()) - 1) ? 0 : currentIndex + 1;

        if (newIndex != currentIndex && newIndex >= 0 && newIndex < static_cast<int>(list.size())) {
            selectedName = list[newIndex];
            loadSelectedEffect(selectedName, lib, currentParams, sys, autoZoom, currentTexture, datPaths);
        }
    }

    static void renderEffectList(const std::vector<std::string>& list, std::string& selectedName, 
                                 const ParticleLibrary& lib, PfxParams& currentParams, 
                                 ParticleSystem& sys, bool autoZoom, Texture2D& currentTexture, 
                                 const std::vector<std::string>& datPaths) 
    {
        for (const auto& n : list) {
            bool isSelected = (n == selectedName);
            std::string label = n + " [" + lib.getOriginFile(n) + "]";
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                if (n != selectedName) {
                    selectedName = n;
                    loadSelectedEffect(selectedName, lib, currentParams, sys, autoZoom, currentTexture, datPaths);
                }
            }
            if (isSelected) ImGui::SetScrollHereY(0.5f);
        }
    }
};