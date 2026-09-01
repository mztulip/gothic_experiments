#pragma once
#include <glm/glm.hpp>
#include <zenkit/World.hh>
#include <zenkit/Stream.hh>
#include <zenkit/Mesh.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/VirtualObject.hh>
#include <string>
#include <cstdint>
#include <cstdlib>

#include "env_load.hpp"

static void testLoadVobMesh(
    const std::shared_ptr<zenkit::VirtualObject>& vob)
{
    if (!vob->show_visual)
        return;

    if (!vob->visual)
        return;

    if (vob->visual->name.empty())
        return;

    if (vob->visual->name[0] == '#')
    {
        printf(
            "POMIJAM WEWNETRZNY VISUAL: '%s'\n",
            vob->visual->name.c_str()
        );

        return;
    }

    if (vob->visual->type !=
        zenkit::VisualType::MESH &&
        vob->visual->type !=
        zenkit::VisualType::MULTI_RESOLUTION_MESH)
    {
        return;
    }

    std::string gothicDir = getGothicDir();

    if (gothicDir.empty())
    {
        printf("Brak GOTHIC2_DIR\n");
        return;
    }

    std::string meshPath =
        findMeshFile(
            gothicDir,
            vob->visual->name);

    if (meshPath.empty())
    {
        printf(
            "MESH NOT FOUND: %s\n",
            vob->visual->name.c_str());

        return;
    }

    printf(
        "MESH FOUND: %s\n",
        meshPath.c_str());

    Mesh3DS mesh;

    if (!Loader3DS::load(meshPath, mesh))
    {
        printf(
            "MESH LOAD FAILED: %s\n",
            meshPath.c_str());

        return;
    }

    printf(
        "  vertices: %zu\n",
        mesh.vertices.size());

    printf(
        "  faces: %zu\n",
        mesh.faces.size());

    if (!mesh.vertices.empty())
    {
        printf(
            "  vertex[0]: %.2f %.2f %.2f\n",
            mesh.vertices[0].x,
            mesh.vertices[0].y,
            mesh.vertices[0].z);
    }

    if (!mesh.faces.empty())
    {
        printf(
            "  face[0]: %u %u %u\n",
            mesh.faces[0].a,
            mesh.faces[0].b,
            mesh.faces[0].c);
    }

    printf(
        "  bounds min: %.2f %.2f %.2f\n",
        mesh.minBounds.x,
        mesh.minBounds.y,
        mesh.minBounds.z
    );

    printf(
        "  bounds max: %.2f %.2f %.2f\n",
        mesh.maxBounds.x,
        mesh.maxBounds.y,
        mesh.maxBounds.z
    );

    printf(
        "  center: %.2f %.2f %.2f\n",
        mesh.center.x,
        mesh.center.y,
        mesh.center.z
    );

    printf(
        "  maxDimension: %.2f\n",
        mesh.maxDimension
    );

    printf(
        "  materials: %zu\n",
        mesh.materials.size()
    );

    if (mesh.materialForFace.size() != mesh.faces.size())
    {
        printf(
            "  WARNING: materialForFace != faces!\n"
        );
    }

    for (size_t i = 0; i < mesh.materialForFace.size(); ++i)
    {
        uint16_t mat = mesh.materialForFace[i];

        if (mat >= mesh.materials.size())
        {
            printf(
                "  WARNING: face[%zu] has invalid material %u\n",
                i,
                mat
            );
        }
    }

    for (size_t i = 0; i < mesh.materials.size(); ++i)
    {
        printf(
            "    material[%zu]: name='%s' texture='%s'\n",
            i,
            mesh.materials[i].name.c_str(),
            mesh.materials[i].textureFile.c_str()
        );
    }

    for (size_t i = 0;
     i < mesh.materialForFace.size();
     ++i)
    {
        printf(
            "    face[%zu] -> material %u\n",
            i,
            mesh.materialForFace[i]
        );
    }

    std::vector<size_t> materialFaceCount(
    mesh.materials.size(),
    0
);

    for (uint16_t mat : mesh.materialForFace)
    {
        if (mat < materialFaceCount.size())
            materialFaceCount[mat]++;
    }

    for (size_t i = 0; i < materialFaceCount.size(); ++i)
    {
        printf(
            "  material[%zu] '%s' -> %zu faces\n",
            i,
            mesh.materials[i].name.c_str(),
            materialFaceCount[i]
        );
    }
}


static void debugLight(const zenkit::VLight& l)
{
    printf("\n========== LIGHT ==========\n");

    printf("position:     %.2f %.2f %.2f\n",
           l.position.x,
           l.position.y,
           l.position.z);

    printf("range:        %.2f\n", l.range);

    printf("color:        %d %d %d\n",
           l.color.r,
           l.color.g,
           l.color.b);

    printf("preset:       '%s'\n", l.preset.c_str());

    printf("is_static:    %s\n", l.is_static ? "true" : "false");

    printf("============================\n");
}


static const char* vobTypeName(zenkit::VirtualObjectType type)
{
    switch (type)
    {
        case zenkit::VirtualObjectType::UNKNOWN:
            return "UNKNOWN";

        case zenkit::VirtualObjectType::zCVob:
            return "zCVob";

        case zenkit::VirtualObjectType::zCVobLevelCompo:
            return "zCVobLevelCompo";

        case zenkit::VirtualObjectType::oCItem:
            return "oCItem";

        case zenkit::VirtualObjectType::oCNpc:
            return "oCNpc";

        case zenkit::VirtualObjectType::zCMoverController:
            return "zCMoverController";

        case zenkit::VirtualObjectType::zCVobScreenFX:
            return "zCVobScreenFX";

        case zenkit::VirtualObjectType::zCVobStair:
            return "zCVobStair";

        case zenkit::VirtualObjectType::zCPFXController:
            return "zCPFXController";

        case zenkit::VirtualObjectType::zCVobAnimate:
            return "zCVobAnimate";

        case zenkit::VirtualObjectType::zCVobLensFlare:
            return "zCVobLensFlare";

        case zenkit::VirtualObjectType::zCVobLight:
            return "zCVobLight";

        case zenkit::VirtualObjectType::zCVobSpot:
            return "zCVobSpot";

        case zenkit::VirtualObjectType::zCVobStartpoint:
            return "zCVobStartpoint";

        case zenkit::VirtualObjectType::zCMessageFilter:
            return "zCMessageFilter";

        case zenkit::VirtualObjectType::zCCodeMaster:
            return "zCCodeMaster";

        case zenkit::VirtualObjectType::zCTriggerWorldStart:
            return "zCTriggerWorldStart";

        case zenkit::VirtualObjectType::zCCSCamera:
            return "zCCSCamera";

        case zenkit::VirtualObjectType::zCCamTrj_KeyFrame:
            return "zCCamTrj_KeyFrame";

        case zenkit::VirtualObjectType::oCTouchDamage:
            return "oCTouchDamage";

        case zenkit::VirtualObjectType::zCTriggerUntouch:
            return "zCTriggerUntouch";

        case zenkit::VirtualObjectType::zCEarthquake:
            return "zCEarthquake";

        case zenkit::VirtualObjectType::oCMOB:
            return "oCMOB";

        case zenkit::VirtualObjectType::oCMobInter:
            return "oCMobInter";

        case zenkit::VirtualObjectType::oCMobBed:
            return "oCMobBed";

        case zenkit::VirtualObjectType::oCMobFire:
            return "oCMobFire";

        case zenkit::VirtualObjectType::oCMobLadder:
            return "oCMobLadder";

        case zenkit::VirtualObjectType::oCMobSwitch:
            return "oCMobSwitch";

        case zenkit::VirtualObjectType::oCMobWheel:
            return "oCMobWheel";

        case zenkit::VirtualObjectType::oCMobContainer:
            return "oCMobContainer";

        case zenkit::VirtualObjectType::oCMobDoor:
            return "oCMobDoor";

        case zenkit::VirtualObjectType::zCTrigger:
            return "zCTrigger";

        case zenkit::VirtualObjectType::zCTriggerList:
            return "zCTriggerList";

        case zenkit::VirtualObjectType::oCTriggerScript:
            return "oCTriggerScript";

        case zenkit::VirtualObjectType::oCTriggerChangeLevel:
            return "oCTriggerChangeLevel";

        case zenkit::VirtualObjectType::oCCSTrigger:
            return "oCCSTrigger";

        case zenkit::VirtualObjectType::zCMover:
            return "zCMover";

        case zenkit::VirtualObjectType::zCVobSound:
            return "zCVobSound";

        case zenkit::VirtualObjectType::zCVobSoundDaytime:
            return "zCVobSoundDaytime";

        case zenkit::VirtualObjectType::oCZoneMusic:
            return "oCZoneMusic";

        case zenkit::VirtualObjectType::oCZoneMusicDefault:
            return "oCZoneMusicDefault";

        case zenkit::VirtualObjectType::zCZoneZFog:
            return "zCZoneZFog";

        case zenkit::VirtualObjectType::zCZoneZFogDefault:
            return "zCZoneZFogDefault";

        case zenkit::VirtualObjectType::zCZoneVobFarPlane:
            return "zCZoneVobFarPlane";

        case zenkit::VirtualObjectType::zCZoneVobFarPlaneDefault:
            return "zCZoneVobFarPlaneDefault";

        default:
            return "UNKNOWN";
    }
}



static const char* visualTypeName(zenkit::VisualType type)
{
    switch (type)
    {
        case zenkit::VisualType::DECAL:
            return "DECAL";

        case zenkit::VisualType::MESH:
            return "MESH";

        case zenkit::VisualType::MULTI_RESOLUTION_MESH:
            return "MULTI_RESOLUTION_MESH";

        case zenkit::VisualType::PARTICLE_EFFECT:
            return "PARTICLE_EFFECT";

        case zenkit::VisualType::AI_CAMERA:
            return "AI_CAMERA";

        case zenkit::VisualType::MODEL:
            return "MODEL";

        case zenkit::VisualType::MORPH_MESH:
            return "MORPH_MESH";

        default:
            return "UNKNOWN";
    }
}


static void vobPrint(const std::shared_ptr<zenkit::VirtualObject>& vob)
{
  printf("\n==============================\n");

  printf("VOB type = %d (%s)\n",
       static_cast<int>(vob->type),
       vobTypeName(vob->type));

    printf("position: %.2f %.2f %.2f\n",
           vob->position.x,
           vob->position.y,
           vob->position.z);

    printf("show_visual: %s\n",
           vob->show_visual ? "true" : "false");

    printf("visual: %s\n",
           vob->visual ? "YES" : "NO");

    if (vob->visual)
    {
        printf("visual ptr: %p\n", (void*)vob->visual.get());
        printf("visual type: %d (%s)\n",
          static_cast<int>(vob->visual->type),
          visualTypeName(vob->visual->type));

        printf("visual name: '%s'\n",
              vob->visual->name.c_str());
    }
    else
    {
        printf("visual: NO\n");
    }

    printf("bbox min: %.2f %.2f %.2f\n",
       vob->bbox.min.x,
       vob->bbox.min.y,
       vob->bbox.min.z);

    printf("bbox max: %.2f %.2f %.2f\n",
          vob->bbox.max.x,
          vob->bbox.max.y,
          vob->bbox.max.z);


    printf("==============================\n");
}

static void walkVobsForDebug(
    const std::shared_ptr<zenkit::VirtualObject>& vob)
{
  vobPrint(vob);
  testLoadVobMesh(vob);


  for (auto& c : vob->children)
      walkVobsForDebug(c);
}