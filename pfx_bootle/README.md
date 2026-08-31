# Gothic Bottle Demo (OpenGL + libepoxy)

Prosty przykład sceny 3D w OpenGL 3.3 core profile:

- **podłoga** z proceduralnie wygenerowaną teksturą "ziemi" (szum wielooktawowy, bez pliku graficznego),
- **butelka** wygenerowana geometrycznie metodą *lathe/revolve* (obrót profilu wokół osi Y), na razie w jednolitym kolorze (butelkowa zieleń),
- **magiczna aura pfx** wokół butelki w stylu Gothic 1/2 — iskierki krążące po orbicie, unoszące się i cyklicznie pojawiające/znikające (fade in/out), renderowane jako point sprite'y z additive blendingiem.

Do ładowania funkcji OpenGL użyto **libepoxy** zamiast GLAD — epoxy nie wymaga jawnej inicjalizacji loadera (nie ma odpowiednika `gladLoadGL`), wystarczy mieć aktywny kontekst OpenGL utworzony przez GLFW.

## Zależności

- CMake ≥ 3.10
- kompilator C++17
- GLFW3 (`libglfw3-dev`)
- libepoxy (`libepoxy-dev`)
- GLM, header-only (`libglm-dev`)

### Instalacja zależności (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libepoxy-dev libglm-dev pkg-config
```

### Arch Linux

```bash
sudo pacman -S cmake glfw-x11 libepoxy glm pkgconf
```

## Budowanie

### Opcja A: `build.sh` (bez CMake, bezpośrednio `g++` + `pkg-config`)

```bash
cd gothic_bottle_demo
chmod +x build.sh        # jeśli bit wykonywalności zgubił się przy pobieraniu
./build.sh                # kompilacja Release (-O2)
./build.sh debug          # kompilacja z symbolami debugowymi
./build.sh run            # kompiluje i od razu uruchamia
./build.sh clean          # usuwa skompilowany plik
```

Skrypt sam sprawdza, czy masz `pkg-config`, `g++`, `epoxy` i `glfw3`, i podpowiada
`apt install ...`, jeśli czegoś brakuje. GLM jest header-only, więc nie jest
linkowany, tylko wymaga widocznych nagłówków (`libglm-dev`).

### Opcja B: CMake

```bash
cd gothic_bottle_demo
mkdir build && cd build
cmake ..
cmake --build . -j
./gothic_bottle_demo
```

## Sterowanie

- `W A S D` — ruch kamery
- `Spacja` / `Left Ctrl` — góra / dół
- Przytrzymaj **prawy przycisk myszy** i poruszaj myszą — rozglądanie się
- Scroll — zoom (FOV)
- `Shift` — przyspieszenie ruchu
- `ESC` — wyjście

## Co dalej / pomysły na rozbudowę

1. **Tekstura butelki** — zamiast `uBaseColor` w `bottleFS`, dodaj `sampler2D` z mapowaniem UV (trzeba dogenerować współrzędne UV w `createBottleMesh`, np. cylindryczne rzutowanie na podstawie kąta `theta` i wysokości profilu).
2. **Przezroczyste szkło** — dodać blending + Fresnel dla bardziej szklanego wyglądu butelki (wymaga sortowania trójkątów albo renderowania w dwóch przebiegach).
3. **Cień pod butelką** — prosty "blob shadow" (przyciemniony okrąg na teksturze podłogi pod butelką) albo pełny shadow mapping.
4. **Dźwięk / interakcja** — podpięcie efektu dźwiękowego przy pojawianiu się aury.
5. **Więcej kolorów aury** — `AuraParticleSystem::respawn` już miesza złoto/błękit; można łatwo dodać zależność koloru od "rodzaju magii" (np. zielona = trucizna, czerwona = ogień — jak różne run/zaklęcia w Gothicu).

## Struktura efektu aury (wzorowana na C_PARTICLEFX z ZenGin)

Efekt magicznej aury nie jest już sterowany "na sztywno" (orbita + sinus), tylko
konfiguracją opisaną strukturą `ParticleFxDef`, która 1:1 odwzorowuje układ pól
znanych ze skryptów PFX oryginalnego silnika Gothica (`instance ... (C_PARTICLEFX)`
w Daedalusie) — patrz komentarz na początku sekcji "Definicja efektu czastek" w
`main.cpp`. Chodzi o pola typu:

| Pole (nasz kod)      | Odpowiednik w ZenGin | Znaczenie |
|-----------------------|----------------------|-----------|
| `ppsValue`             | `ppsValue`            | ile cząstek na sekundę emituje efekt |
| `shpType/shpFOR/shpDim/shpOffsetVec` | `shp...`  | kształt i pozycja emitera (tu: sfera wokół szyjki butelki) |
| `dirMode/dirAngleHead(Var)/dirAngleElev(Var)` | `dir...` | kierunek wylotu cząstki (azymut + elewacja + losowość) |
| `velAvg/velVar`        | `velAvg/velVar`       | prędkość początkowa cząstki (średnia + wariancja) |
| `lspPartAvg/lspPartVar`| `lspPartAvg/lspPartVar` | czas życia pojedynczej cząstki |
| `flyGravity`           | `flyGravity`          | "grawitacja" działająca na cząstkę w locie |
| `visSizeStart/visSizeEndScale` | `visSizeStart/visSizeEndScale` | rozmiar cząstki na początku i pod koniec życia |
| `visAlphaFunc/visAlphaStart/visAlphaEnd` | `visAlphaFunc/visAlphaStart/visAlphaEnd` | tryb mieszania (ADD = świecenie) i zanik przezroczystości |
| `visTexColorStart/visTexColorEnd` | `visTexColor...` | zmiana koloru cząstki w trakcie życia (złoty -> błękitny) |

Klasa `ZenParticleFX` działa jak silnikowy "emitter runtime" (`zCParticleFX`):
co klatkę emituje `ppsValue * dt` nowych cząstek z puli, każda dostaje losowy
kierunek/prędkość/czas życia wyliczony z par średnia+wariancja, a rozmiar/alfa/
kolor są interpolowane w trakcie życia cząstki wg krzywych z definicji efektu.
Dzięki temu efekt "pojawiania się i znikania iskierek" wynika naturalnie z
ciągłej emisji + zanikania alfa (`visAlphaStart -> visAlphaEnd`), a nie ze
sztucznej funkcji sinus jak w poprzedniej wersji.

**Uwaga o wierności odwzorowania:** dokładne nazwy pól i ich domyślne wartości
w prawdziwych plikach PFX Gothica mogą się nieco różnić (nie mam dostępu do
oryginalnych źródeł silnika w tej chwili) — powyższe odtworzyłem z pamięci na
podstawie ogólnie znanej dokumentacji modderskiej ZenGin. Układ koncepcyjny
(emitter shape + dir + vel + lifespan + krzywe vis) jest jednak wierny realnej
architekturze systemu PFX z Gothic 1/2, więc jeśli chcesz, żeby efekt
odpowiadał konkretnemu, znanemu Ci plikowi `.pfx` z gry — podaj jego zawartość,
a przemapuję `ParticleFxDef` dokładnie pod te wartości.

Żeby dodać kolejny efekt (np. inny dla różnych przedmiotów), wystarczy napisać
nową funkcję w stylu `makePfxMagicAuraBottle()` z innymi wartościami — dokładnie
tak, jak w Gothicu dodawało się nową `instance` w pliku `.d`.

## Uwaga dot. epoxy vs GLAD

W kodzie nie ma żadnego `gladLoadGLLoader(...)` — to jest właśnie różnica względem GLAD. Wystarczy:

```cpp
glfwMakeContextCurrent(window);
// od tego momentu można normalnie wołać funkcje glXxx() — epoxy samo
// znajduje właściwy wskaźnik funkcji przy pierwszym wywołaniu.
```

Plik nagłówkowy do include'owania to `<epoxy/gl.h>` (zamiast `<glad/glad.h>`), i **musi** być dołączony przed `<GLFW/glfw3.h>` (tak jak w `main.cpp`).
