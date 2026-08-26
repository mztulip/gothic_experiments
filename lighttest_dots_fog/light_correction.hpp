#pragma once

#include <cstdio>

struct RangeMapPoint {
    float original;
    float corrected;
};


static const RangeMapPoint RANGE_MAP[] = {
  {   15.f,  100.f }, 
  {   50.f,  100.f },  
  {   80.f,  100.f },  // NW_STANDART_DARKBLUE
  {  100.f,  100.f },  // FIRESMALL, AURA
  {  150.f,  100.f },  // DEFAULTLIGHT_DARKBLUE
  {  200.f,  200.f },  // CITY
  {  250.f,  200.f },  // NW_STANDART_DARKBLUE, NW_STANDART_FIRE_DYNAMIC
  {  300.f,  200.f },  // NW_STANDART_CRAWLER, AMBIENCE_300, DEMONTOWER_SMALL_LIGHT
  {  350.f,  200.f },  // AMBIENCE_ENTRANCE_500, DARK
  {  400.f,  200.f },  // AMBIENCE_IN_FRONT_OF_WINDOW, AMBIENCE_500, DARKROOMBLUE
  {  500.f,  200.f },  // FIRE_STAT, CRYSTAL_01, FACKEL_FEUER, BANDITEN, FX_LIGHT1
  {  600.f,  200.f },  // VALLEY_DUNGEON_600, CRYSTAL_ROSE_600, FACKEL_FEUER
  {  650.f,  200.f },  // AURA (small)
  {  700.f,  200.f },  // FIRE_SMALL_02, DEMONTOWER_LIGHT_02, LIGHT, HELL_RED
  {  800.f,  200.f },  // NW_STANDART_FIRE_STATIC, HELLRED_DYN
  {  900.f,  200.f },  // NW_STANDART_FIRE_STATIC, FIRE_STAT
  { 1000.f,  300.f },  // FIRESMALL, CRYSTAL_02, DARK_CANYON_1000
  { 1200.f,  300.f },  // LIGHT
  { 1500.f,  300.f },  // INROOM_DARKBLUE, HELLES FEUER
  { 2000.f,  300.f },  // NW_STANDART_DARKBLUE
  { 3000.f,  300.f },  // AURA (large), TEST
};


static float correctedRange(float range)
{
    constexpr size_t count =
        sizeof(RANGE_MAP) / sizeof(RANGE_MAP[0]);

    if(range <= RANGE_MAP[0].original)
        return RANGE_MAP[0].corrected;

    if(range >= RANGE_MAP[count - 1].original)
        return RANGE_MAP[count - 1].corrected;

    for(size_t i = 1; i < count; ++i)
    {
        if(range <= RANGE_MAP[i].original)
        {
            const auto& a = RANGE_MAP[i - 1];
            const auto& b = RANGE_MAP[i];

            float t = (range - a.original) /
                      (b.original - a.original);

            return a.corrected +
                   t * (b.corrected - a.corrected);
        }
    }

    return range;
}