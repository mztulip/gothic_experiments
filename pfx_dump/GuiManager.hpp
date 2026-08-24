#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <variant>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "ParticleLibrary.hpp"
#include "ParticleSystem.hpp"
#include "PfxParams.hpp"
#include "VfxParams.hpp"
#include "texture_loader.h"

// Kontener przechowujący jeden z dwóch niezależnych typów efektów
using ActiveEffectParams = std::variant<std::monostate, PfxParams, VfxParams>;

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
                                   ActiveEffectParams& activeParams, ParticleSystem& sys, 
                                   bool autoZoom, Texture2D& currentTexture, 
                                   const std::vector<std::string>& datPaths) 
    {
        activeParams = std::monostate{};
        sys.clear();
        currentTexture.free();

        bool isVFX = containsIgnoreCase(lib.getOriginFile(name), "VISUALFX");

        if (isVFX) {
            VfxParams vfx;
            if (!lib.getVfxParams(name, vfx)) return;

            std::string texToLoad = vfx.visName;
            if (!containsIgnoreCase(texToLoad, ".3ds") && !containsIgnoreCase(texToLoad, ".mms")) {
                loadTexture(texToLoad, vfx.loadedTexturePath, currentTexture, datPaths);
            }

            activeParams = vfx;
        } else {
            PfxParams pfx;
            if (!lib.getPfxParams(name, pfx)) return;

            if (autoZoom) ParticleSystem::reframeCamera(pfx);

            loadTexture(pfx.visName, pfx.loadedTexturePath, currentTexture, datPaths);
            activeParams = pfx;
        }
    }

    static void drawUI(const ParticleLibrary& lib, std::string& selectedName, 
                       ActiveEffectParams& activeParams, ParticleSystem& sys, 
                       bool& autoZoom, Texture2D& currentTexture, 
                       const std::vector<std::string>& datPaths) 
    {
        static char searchBuffer[128] = "";
        std::string searchFilter(searchBuffer);
        
        std::vector<std::string> filteredPfx;
        std::vector<std::string> filteredVfx;

        for (const auto& name : lib.names()) {
            if (containsIgnoreCase(name, searchFilter)) {
                if (containsIgnoreCase(lib.getOriginFile(name), "VISUALFX")) {
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
        
        if (!selectedName.empty() && ImGui::Button("Dopasuj")) {
            if (auto* pfx = std::get_if<PfxParams>(&activeParams)) {
                ParticleSystem::reframeCamera(*pfx);
            }
        }

        ImGui::InputText("##szukaj", searchBuffer, IM_ARRAYSIZE(searchBuffer));
        ImGui::SameLine();
        if (ImGui::Button("X")) searchBuffer[0] = '\0';

        ImGui::Separator();

        if (ImGui::BeginTabBar("EffectTypeTabs")) {
            if (ImGui::BeginTabItem("ParticleFX")) {
                bool scrollToSelected = handleNavKeys(filteredPfx, selectedName, lib, activeParams, sys, autoZoom, currentTexture, datPaths);
                
                ImGui::BeginChild("lista_pfx");
                renderEffectList(filteredPfx, selectedName, lib, activeParams, sys, autoZoom, currentTexture, datPaths, scrollToSelected);
                ImGui::EndChild();
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("VisualFX")) {
                bool scrollToSelected = handleNavKeys(filteredVfx, selectedName, lib, activeParams, sys, autoZoom, currentTexture, datPaths);
                
                ImGui::BeginChild("lista_vfx");
                renderEffectList(filteredVfx, selectedName, lib, activeParams, sys, autoZoom, currentTexture, datPaths, scrollToSelected);
                ImGui::EndChild();
                
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        /* Panel Szczegółów (Inspektor) */
        ImGui::SetNextWindowPos(ImVec2(350, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
        
        if (ImGui::Begin("Inspector Szczegolow Efektu"))
        {
            if (!selectedName.empty()) 
            {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Nazwa Instancji:"); ImGui::SameLine(); ImGui::Text("%s", selectedName.c_str());
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Plik Skryptu (.DAT):"); ImGui::SameLine(); ImGui::Text("%s", lib.getOriginFile(selectedName).c_str());
                ImGui::Separator();

                // Wyświetlanie właściwego widoku w zależności od wariantu
                std::visit([&](auto&& param) {
                    using T = std::decay_t<decltype(param)>;
                    if constexpr (std::is_same_v<T, PfxParams>) 
                    {
                        drawPfxInspector(param, currentTexture);
                    } 
                    else if constexpr (std::is_same_v<T, VfxParams>) 
                    {
                        drawVfxInspector(param, currentTexture);
                    }
                    else 
                    {
                        std::cout<<"ehh";
                    }
                }, activeParams);

            }
            else 
            {
                ImGui::TextDisabled("Wybierz efekt z listy, aby wyswietlic szczegoly.");
            }
        }
        ImGui::End();
    }

private:
    static void loadTexture(const std::string& visName, std::string& loadedPathOut, Texture2D& currentTexture, const std::vector<std::string>& datPaths) {
        if (visName.empty()) return;
        std::string gothicDir = ParticleLibrary::resolveGothicDir(datPaths);
        std::string fullPath = TextureLoader::resolveGothicTexturePath(visName, gothicDir);

        if (!fullPath.empty()) {
            currentTexture = TextureLoader::loadFromFile(fullPath, false);
            if (currentTexture.valid) {
                loadedPathOut = fullPath;
            }
        }
    }

    static void drawVfxInspector(const VfxParams& vfx, const Texture2D& tex) {
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 1.0f, 1.0f), "--- VISUAL FX (DAEDALUS) ---");
        std::cout<<"Inspector"<<std::endl;
        
        // Klasyfikacja po rozszerzeniu
        if (containsIgnoreCase(vfx.visName, ".3ds")) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.6f, 1.0f), "Typ zasobu: Siatka 3D (.3DS)");
        } else if (containsIgnoreCase(vfx.visName, ".pfx")) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Typ zasobu: Podpięty PFX (.PFX)");
        } else if (containsIgnoreCase(vfx.visName, ".mms")) {
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "Typ zasobu: Morph Mesh (.MMS)");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Typ zasobu: Kontener/Inne");
        }

        ImGui::Text("Plik Wizualny: %s", vfx.visName.empty() ? "(Brak)" : vfx.visName.c_str());
        if (!vfx.visSize.empty())  ImGui::Text("Skala (visSize): %s", vfx.visSize.c_str());
        if (!vfx.visAlpha.empty()) ImGui::Text("Alpha (visAlpha): %s", vfx.visAlpha.c_str());

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "--- TRAJEKTORIA I CZAS ---");
        if (!vfx.emTrjMode.empty())        ImGui::Text("Tryb (emTrjMode): %s", vfx.emTrjMode.c_str());
        if (!vfx.emTrjOriginNode.empty())  ImGui::Text("Node Startowy: %s", vfx.emTrjOriginNode.c_str());
        if (!vfx.emTrjTargetNode.empty())  ImGui::Text("Node Celu: %s", vfx.emTrjTargetNode.c_str());
        // if (!vfx.emCheckCollision.empty()) ImGui::Text("Kolizje: %s", vfx.emCheckCollision.c_str());

        // ImGui::Text("Opóźnienie PFX: %.2f s", vfx.emPfxDelay);
        // ImGui::Text("Skalowanie Czasu: %.2f s", vfx.emScaleDuration);

        // if (!vfx.lightPresetName.empty() || !vfx.lightRange.empty()) {
        //     ImGui::Separator();
        //     ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "--- EFEKTY ŚWIATŁA ---");
        //     if (!vfx.lightPresetName.empty()) ImGui::Text("Preset Światła: %s", vfx.lightPresetName.c_str());
        //     if (!vfx.lightRange.empty())      ImGui::Text("Zasięg Światła: %s", vfx.lightRange.c_str());
        //     if (!vfx.lightColor.empty())      ImGui::Text("Kolor Światła: %s", vfx.lightColor.c_str());
        // }

        if (!vfx.userString.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "--- USER STRING ---");
            ImGui::TextWrapped("%s", vfx.userString.c_str());
        }

        if (tex.valid && tex.id != 0) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Podgląd Tekstury 2D:");
            ImGui::Image((void*)(intptr_t)tex.id, ImVec2(96, 96));
        }
    }

    static void drawPfxInspector(const PfxParams& pfx, const Texture2D& tex) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "--- PARTICLE FX (C_PARTICLEFX) ---");

        ImGui::TextUnformatted("--- TEKSTURA ---");
        ImGui::Text("Plik w Daedalus: %s", pfx.visName.empty() ? "(Brak)" : pfx.visName.c_str());

        if (!pfx.loadedTexturePath.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Ścieżka:");
            ImGui::TextWrapped("%s", pfx.loadedTexturePath.c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Stan: Brak tekstury / Niewykryta");
        }

        if (tex.valid && tex.id != 0) {
            ImGui::Text("Wymiary: %dx%d px", tex.width, tex.height);
            ImGui::Image((void*)(intptr_t)tex.id, ImVec2(80, 80));
        }

        ImGui::Separator();
        ImGui::TextUnformatted("--- PARAMETRY EMISJI ---");
        ImGui::Text("Szybkość Emisji (ppsValue): %.2f", pfx.ppsValue);
        ImGui::Text("Kształt Emitera (shpType): %s", pfx.shpType.c_str());
        ImGui::Text("Wymiary Emitera: [%.1f, %.1f, %.1f]", pfx.shpDim.x, pfx.shpDim.y, pfx.shpDim.z);
        ImGui::Text("Prędkość Cząsteczek: %.1f (+-%.1f)", pfx.velAvg, pfx.velVar);
        ImGui::Text("Czas Życia: %.0f ms (+-%.0f ms)", pfx.lspAvg, pfx.lspVar);
        ImGui::Text("Grawitacja: [%.1f, %.1f, %.1f]", pfx.gravity.x, pfx.gravity.y, pfx.gravity.z);
        ImGui::Text("Skala Rozmiaru: [%.1f,%.1f] -> x%.2f", pfx.sizeStart.x, pfx.sizeStart.y, pfx.sizeEndScale);
    }

    static bool handleNavKeys(const std::vector<std::string>& list, std::string& selectedName, 
                              const ParticleLibrary& lib, ActiveEffectParams& activeParams, 
                              ParticleSystem& sys, bool autoZoom, Texture2D& currentTexture, 
                              const std::vector<std::string>& datPaths) 
    {
        if (ImGui::GetIO().WantTextInput || list.empty()) return false;

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
            loadSelectedEffect(selectedName, lib, activeParams, sys, autoZoom, currentTexture, datPaths);
            return true;
        }
        return false;
    }

    static void renderEffectList(const std::vector<std::string>& list, std::string& selectedName, 
                                 const ParticleLibrary& lib, ActiveEffectParams& activeParams, 
                                 ParticleSystem& sys, bool autoZoom, Texture2D& currentTexture, 
                                 const std::vector<std::string>& datPaths, bool scrollToSelected) 
    {
        for (const auto& n : list) {
            bool isSelected = (n == selectedName);
            std::string label = n + " [" + lib.getOriginFile(n) + "]";
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                if (n != selectedName) {
                    selectedName = n;
                    loadSelectedEffect(selectedName, lib, activeParams, sys, autoZoom, currentTexture, datPaths);
                }
            }
            if (isSelected && scrollToSelected) {
                ImGui::SetScrollHereY(0.5f);
            }
        }
    }
};