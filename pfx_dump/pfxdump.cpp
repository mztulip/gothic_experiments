/*
   pfxdump - wczytuje PARTICLEFX.DAT (skompilowany skrypt Daedalus z Gothic/
   Gothic II) i wypisuje wszystkie parametry pojedynczego efektu czasteczkowego
   (np. iskierki wokol mikstury) po jego nazwie.

   Mechanizm: efekty czasteczkowe NIE sa osobnymi plikami .PFX w sensie
   samodzielnego formatu binarnego - kazdy .PFX to w rzeczywistosci nazwana
   INSTANCJA klasy C_PARTICLEFX wewnatrz jednego, wspolnego, skompilowanego
   skryptu Daedalus (PARTICLEFX.DAT). Zeby dostac konkretny efekt, trzeba:
     1. wczytac caly skrypt do zenkit::DaedalusScript,
     2. zarejestrowac wiazanie klasy C_PARTICLEFX <-> C++ struct przez
        zenkit::IParticleEffect::register_(),
     3. odpalic Daedalus VM i "zainicjalizowac" instancje o danej nazwie -
        to uruchamia bajtkod, ktory wypelnia pola struct wartosciami z
        oryginalnego zrodla efektu.

   Dokladnie tak samo robi to OpenGothic w
   game/game/definitions/particlesdefinitions.cpp (ParticlesDefinitions::implGetDirect).

   Uzycie:
     ./pfxdump PARTICLEFX.DAT PFX_IT_MAGICSPARKLE
     ./pfxdump "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/_Work/Data/Scripts/_compiled/PARTICLEFX.DAT" PFX_IT_MAGICSPARKLE

   Jesli nie znasz dokladnej nazwy efektu, uruchom z flaga --list, zeby
   wypisac wszystkie symbole klasy C_PARTICLEFX zdefiniowane w pliku:
     ./pfxdump PARTICLEFX.DAT --list
*/

#include <zenkit/DaedalusScript.hh>
#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>
#include <zenkit/Stream.hh>

#include <cstdio>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

static void printEffect(const std::string& name, const zenkit::IParticleEffect& p)
{
  printf("=== %s ===\n\n", name.c_str());

  printf("-- Emisja (pps = particles per second) --\n");
  printf("  pps_value            = %f\n", p.pps_value);
  printf("  pps_scale_keys_s     = \"%s\"\n", p.pps_scale_keys_s.c_str());
  printf("  pps_is_looping       = %d\n", p.pps_is_looping);
  printf("  pps_is_smooth        = %d\n", p.pps_is_smooth);
  printf("  pps_fps              = %f\n", p.pps_fps);
  printf("  pps_create_em_s      = \"%s\"\n", p.pps_create_em_s.c_str());
  printf("  pps_create_em_delay  = %f\n", p.pps_create_em_delay);

  printf("\n-- Ksztalt emitera (shp) --\n");
  printf("  shp_type_s           = \"%s\"\n", p.shp_type_s.c_str());
  printf("  shp_for_s            = \"%s\"\n", p.shp_for_s.c_str());
  printf("  shp_offset_vec_s     = \"%s\"\n", p.shp_offset_vec_s.c_str());
  printf("  shp_distrib_type_s   = \"%s\"\n", p.shp_distrib_type_s.c_str());
  printf("  shp_distrib_walk_speed = %f\n", p.shp_distrib_walk_speed);
  printf("  shp_is_volume        = %d\n", p.shp_is_volume);
  printf("  shp_dim_s            = \"%s\"\n", p.shp_dim_s.c_str());
  printf("  shp_mesh_s           = \"%s\"\n", p.shp_mesh_s.c_str());
  printf("  shp_mesh_render_b    = %d\n", p.shp_mesh_render_b);
  printf("  shp_scale_keys_s     = \"%s\"\n", p.shp_scale_keys_s.c_str());
  printf("  shp_scale_is_looping = %d\n", p.shp_scale_is_looping);
  printf("  shp_scale_is_smooth  = %d\n", p.shp_scale_is_smooth);
  printf("  shp_scale_fps        = %f\n", p.shp_scale_fps);

  printf("\n-- Kierunek (dir) --\n");
  printf("  dir_mode_s           = \"%s\"\n", p.dir_mode_s.c_str());
  printf("  dir_for_s            = \"%s\"\n", p.dir_for_s.c_str());
  printf("  dir_mode_target_for_s = \"%s\"\n", p.dir_mode_target_for_s.c_str());
  printf("  dir_mode_target_pos_s = \"%s\"\n", p.dir_mode_target_pos_s.c_str());
  printf("  dir_angle_head       = %f\n", p.dir_angle_head);
  printf("  dir_angle_head_var   = %f\n", p.dir_angle_head_var);
  printf("  dir_angle_elev       = %f\n", p.dir_angle_elev);
  printf("  dir_angle_elev_var   = %f\n", p.dir_angle_elev_var);

  printf("\n-- Predkosc i czas zycia --\n");
  printf("  vel_avg              = %f\n", p.vel_avg);
  printf("  vel_var              = %f\n", p.vel_var);
  printf("  lsp_part_avg         = %f  (czas zycia czasteczki, ms)\n", p.lsp_part_avg);
  printf("  lsp_part_var         = %f\n", p.lsp_part_var);

  printf("\n-- Fizyka --\n");
  printf("  fly_gravity_s        = \"%s\"\n", p.fly_gravity_s.c_str());
  printf("  fly_colldet_b        = %d\n", p.fly_colldet_b);

  printf("\n-- Wyglad (vis) --\n");
  printf("  vis_name_s           = \"%s\"  (tekstura czasteczki)\n", p.vis_name_s.c_str());
  printf("  vis_orientation_s    = \"%s\"\n", p.vis_orientation_s.c_str());
  printf("  vis_tex_is_quadpoly  = %d\n", p.vis_tex_is_quadpoly);
  printf("  vis_tex_ani_fps      = %f\n", p.vis_tex_ani_fps);
  printf("  vis_tex_ani_is_looping = %d\n", p.vis_tex_ani_is_looping);
  printf("  vis_tex_color_start_s = \"%s\"\n", p.vis_tex_color_start_s.c_str());
  printf("  vis_tex_color_end_s  = \"%s\"\n", p.vis_tex_color_end_s.c_str());
  printf("  vis_size_start_s     = \"%s\"\n", p.vis_size_start_s.c_str());
  printf("  vis_size_end_scale   = %f\n", p.vis_size_end_scale);
  printf("  vis_alpha_func_s     = \"%s\"  (np. ADD = addytywny blending)\n", p.vis_alpha_func_s.c_str());
  printf("  vis_alpha_start      = %f\n", p.vis_alpha_start);
  printf("  vis_alpha_end        = %f\n", p.vis_alpha_end);

  printf("\n-- Slady (trail) --\n");
  printf("  trl_fade_speed       = %f\n", p.trl_fade_speed);
  printf("  trl_texture_s        = \"%s\"\n", p.trl_texture_s.c_str());
  printf("  trl_width            = %f\n", p.trl_width);

  printf("\n-- Znacznik (marker) --\n");
  printf("  mrk_fades_peed       = %f\n", p.mrk_fades_peed);
  printf("  mrkt_exture_s        = \"%s\"\n", p.mrkt_exture_s.c_str());
  printf("  mrk_size             = %f\n", p.mrk_size);

  printf("\n-- Roznosci --\n");
  printf("  flock_mode           = \"%s\"\n", p.flock_mode.c_str());
  printf("  flock_strength       = %f\n", p.flock_strength);
  printf("  use_emitters_for     = %d\n", p.use_emitters_for);
  printf("  time_start_end_s     = \"%s\"\n", p.time_start_end_s.c_str());
  printf("  m_bis_ambient_pfx    = %d\n", p.m_bis_ambient_pfx);
  }

/* Wypisuje nazwy wszystkich symboli w skrypcie, ktore sa instancjami klasy
   C_PARTICLEFX - przydatne, zeby znalezc dokladna nazwe efektu bez
   przekopywania sie przez skrypty Daedalus recznie. */
static void listEffects(zenkit::DaedalusScript& script)
{
  auto* cls = script.find_symbol_by_name("C_PARTICLEFX");
  if(cls==nullptr)
  {
    fprintf(stderr, "Nie znaleziono klasy C_PARTICLEFX w skrypcie.\n");
    return;
  }

  std::vector<std::string> names;
  for(auto& sym : script.symbols())
  {
    if(sym.type()==zenkit::DaedalusDataType::INSTANCE && sym.parent()==cls->index())
      names.push_back(sym.name());
  }

  std::sort(names.begin(), names.end());
  printf("Znaleziono %zu efektow czasteczkowych:\n\n", names.size());
  for(auto& n : names)
    printf("  %s\n", n.c_str());
  }

int main(int argc, char** argv)
{
  if(argc<3)
  {
    fprintf(stderr, "uzycie: %s ParticleFx.dat NAZWA_EFEKTU\n", argv[0]);
    fprintf(stderr, "        %s ParticleFx.dat --list\n", argv[0]);
    fprintf(stderr, "przyklad: %s ParticleFx.dat PFX_IT_MAGICSPARKLE\n", argv[0]);
    return 1;
  }

  std::string datPath = argv[1];
  std::string fxName  = argv[2];

  zenkit::DaedalusScript script;
  try
  {
    auto reader = zenkit::Read::from(datPath);
    script.load(reader.get());
  }
  catch(const std::exception& e)
  {
    fprintf(stderr, "Nie udalo sie wczytac %s: %s\n", datPath.c_str(), e.what());
    return 1;
  }

  /* Rejestrujemy wiazanie C_PARTICLEFX <-> zenkit::IParticleEffect - bez
     tego VM nie wie, jak zmapowac pola skryptu na nasza strukture C++.
     NIE rejestrujemy IParticleEffectEmitKey (C_PARTICLEFXEMITKEY) - ta klasa
     bywa niepelna/nieobecna w niektorych wersjach PARTICLEFX.DAT i register_()
     rzuca wtedy zenkit::DaedalusSymbolNotFound. Nie jest nam potrzebna do
     wczytywania zwyklych efektow (--list oraz wypisanie pojedynczego efektu),
     wiec pomijamy ja calkowicie zamiast walczyc z brakujacymi polami. */
  try
  {
    zenkit::IParticleEffect::register_(script);
  }
  catch(const std::exception& e)
  {
    fprintf(stderr, "Nie udalo sie zarejestrowac C_PARTICLEFX: %s\n", e.what());
    return 1;
  }

  if(fxName=="--list")
  {
    listEffects(script);
    return 0;
  }

  zenkit::DaedalusVm vm(std::move(script), zenkit::DaedalusVmExecutionFlag::ALLOW_NULL_INSTANCE_ACCESS);

  auto* sym = vm.find_symbol_by_name(fxName);
  if(sym==nullptr)
  {
    fprintf(stderr, "Nie znaleziono efektu \"%s\" w %s\n", fxName.c_str(), datPath.c_str());
    fprintf(stderr, "Wskazowka: uruchom z '--list' zamiast nazwy efektu, zeby zobaczyc dostepne nazwy.\n");
    return 1;
  }

  auto pfx = std::make_shared<zenkit::IParticleEffect>();
  pfx->vis_tex_is_quadpoly = 1; /* domyslna wartosc, tak samo jak w OpenGothic */

  try
  {
    vm.init_instance(pfx, sym);
  }
  catch(const std::exception& e)
  {
    fprintf(stderr, "Blad podczas inicjalizacji instancji \"%s\": %s\n", fxName.c_str(), e.what());
    return 1;
  }

  printEffect(fxName, *pfx);
  return 0;
  }
