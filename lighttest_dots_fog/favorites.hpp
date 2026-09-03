#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <filesystem>

#include "camera.hpp"
#include "imgui.h"

struct FavoritePosition
{
    std::string world;
    std::string name;
    glm::vec3   pos;
    float       yaw;
    float       pitch;
};

static std::vector<FavoritePosition> g_favorites;
static const char* FAVORITES_PATH = "favorites.json";
static std::string g_currentWorld; // ustawiane raz w main(), np. "newworld"

static std::string worldIdFromZen(const std::string& zenPath)
{
    if (zenPath.empty())
        return "demo";

    std::filesystem::path p(zenPath);
    std::string stem = p.stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return stem;
}

// ---------------------------------------------------------------
// Zapis - piszemy sami, wiec format jest w 100% pod nasza kontrola
// ---------------------------------------------------------------
static std::string escapeJson(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

static void saveFavorites()
{
    std::ofstream f(FAVORITES_PATH);
    if (!f.is_open())
    {
        fprintf(stderr, "Nie mozna zapisac %s\n", FAVORITES_PATH);
        return;
    }

    f << "[\n";
    for (size_t i = 0; i < g_favorites.size(); ++i)
    {
        const auto& p = g_favorites[i];
        f << "  {\"world\":\"" << escapeJson(p.world) << "\","
            << "\"name\":\"" << escapeJson(p.name) << "\","
            << "\"x\":" << p.pos.x << ",\"y\":" << p.pos.y << ",\"z\":" << p.pos.z << ","
            << "\"yaw\":" << p.yaw << ",\"pitch\":" << p.pitch << "}";
        if (i + 1 < g_favorites.size()) f << ",";
        f << "\n";
    }
    f << "]\n";
}

// ---------------------------------------------------------------
// Odczyt - prosty skaner tekstowy dopasowany do formatu powyzej
// (nie jest to ogolny parser JSON, tylko pod nasz wlasny zapis)
// ---------------------------------------------------------------
static std::string extractStringField(const std::string& obj, const std::string& key)
{
    std::string pat = "\"" + key + "\":\"";
    size_t p = obj.find(pat);
    if (p == std::string::npos) return {};
    p += pat.size();
    size_t end = obj.find('"', p);
    while (end != std::string::npos && obj[end - 1] == '\\') // pomijamy escapowany cudzyslow
        end = obj.find('"', end + 1);
    if (end == std::string::npos) return {};
    return obj.substr(p, end - p);
}

static float extractFloatField(const std::string& obj, const std::string& key)
{
    std::string pat = "\"" + key + "\":";
    size_t p = obj.find(pat);
    if (p == std::string::npos) return 0.f;
    p += pat.size();
    return std::strtof(obj.c_str() + p, nullptr);
}

static void loadFavorites()
{
    g_favorites.clear();

    std::ifstream f(FAVORITES_PATH);
    if (!f.is_open())
        return; // brak pliku - to normalne przy pierwszym uruchomieniu

    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    size_t pos = 0;
    while (true)
    {
        size_t start = content.find('{', pos);
        if (start == std::string::npos) break;
        size_t end = content.find('}', start);
        if (end == std::string::npos) break;

        std::string obj = content.substr(start, end - start + 1);

        FavoritePosition fp;
        fp.world = extractStringField(obj, "world");
        fp.name  = extractStringField(obj, "name");
        fp.pos.x = extractFloatField(obj, "x");
        fp.pos.y = extractFloatField(obj, "y");
        fp.pos.z = extractFloatField(obj, "z");
        fp.yaw   = extractFloatField(obj, "yaw");
        fp.pitch = extractFloatField(obj, "pitch");

        g_favorites.push_back(fp);
        pos = end + 1;
    }
}

// ---------------------------------------------------------------
// Okno ImGui
// ---------------------------------------------------------------
static void drawFavoritesWindow(Camera& cam)
{
    static char nameBuf[128] = "";

    ImGui::Begin("Ulubione pozycje");

    ImGui::Text("Swiat: %s", g_currentWorld.c_str());
    ImGui::InputText("Nazwa", nameBuf, sizeof(nameBuf));

    if (ImGui::Button("Zapisz aktualna pozycje"))
    {
        FavoritePosition fp;
        fp.world = g_currentWorld;   // <-- kluczowe
        fp.pos   = cam.pos;
        fp.yaw   = cam.yaw;
        fp.pitch = cam.pitch;

        if (nameBuf[0] != '\0')
        {
            fp.name = nameBuf;
        }
        else
        {
            // licz numer tylko wsrod wpisow TEGO swiata, nie wszystkich
            size_t countForWorld = 0;
            for (auto& f : g_favorites)
                if (f.world == g_currentWorld) ++countForWorld;

            char autoName[64];
            snprintf(autoName, sizeof(autoName), "Pozycja %zu (%.0f, %.0f, %.0f)",
                     countForWorld + 1, fp.pos.x, fp.pos.y, fp.pos.z);
            fp.name = autoName;
        }

        g_favorites.push_back(fp);
        nameBuf[0] = '\0';
        saveFavorites();
    }

    ImGui::Separator();

    int toRemove = -1;
    for (int i = 0; i < int(g_favorites.size()); ++i)
    {
        if (g_favorites[i].world != g_currentWorld)
            continue;   // <-- filtr: pokazuj tylko biezaca mape

        ImGui::PushID(i);

        if (ImGui::Button(g_favorites[i].name.c_str()))
        {
            cam.pos   = g_favorites[i].pos;
            cam.yaw   = g_favorites[i].yaw;
            cam.pitch = g_favorites[i].pitch;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X"))
        {
            toRemove = i;
        }

        ImGui::PopID();
    }

    if (toRemove >= 0)
    {
        g_favorites.erase(g_favorites.begin() + toRemove);
        saveFavorites();
    }

    ImGui::End();
}