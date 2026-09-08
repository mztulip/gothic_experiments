# zEngine VDF Explorer / Viewer

A lightweight GUI application built with C++20, **ZenKit**, **ImGui**, and **GLFW/OpenGL3** designed for browsing and extracting files from Virtual File System (`.VDF`) archives used by the zEngine (Gothic / Gothic II) on Linux.

It features automatic VDF mounting from the `Data/` directory, live file searching/filtering, resource extraction, and a console-only text tree dump mode.

---

## 🚀 Features

* **VFS Tree Viewer:** Browse the directory hierarchy and contents of all mounted `.VDF` archives simultaneously.
* **Live File Filter:** Search files in real time (automatically expands folders containing matching entries).
* **GUI Extraction:** Right-click context menu on any file to extract it directly to disk.
* **CLI Tree Dump (`-tree`):** Instantly print the complete file structure tree to stdout without starting a graphical window.
* **Environment Variable Support:** Automatically loads the Gothic path from `GOTHIC2_DIR`.
* **Quick Exit:** Press `ESC` at any time to exit the application.

---

## 🛠️ Prerequisites & Dependencies

Ensure you have the following packages installed on your system:

* C++20 compatible compiler (`GCC` or `Clang`)
* `CMake` (v3.15 or newer)
* `Git`
* GLFW and OpenGL development libraries (e.g., `glfw-x11` / `glfw-wayland`, `libepoxy` on Arch Linux)

---

## 📂 Project Structure

The build script expects the Git submodules (`ZenKit` and `imgui`) to be present in the parent directory level:

```text
.
├── build.sh            # Bash build script
├── main.cpp            # Core application logic and ImGui GUI
├── vfs_loader.hpp      # Automatic VDF loader from /Data
├── README.md
├── ../ZenKit/          # ZenKit Git submodule
└── ../imgui/           # ImGui Git submodule (with GLFW & OpenGL3 backends)

## 💻 Usage

### 1. Environment Variable (Recommended)
Set the path to your Gothic or Gothic II root directory (the folder containing the `Data/` subfolder) in your environment:

```bash
export GOTHIC2_DIR="/home/mz/JoWood/Gothic II"
./vfsexplorer

export GOTHIC2_DIR="/home/mz/JoWood/Gothic II"
./vfsexplorer -tree

./vfsexplorer "/home/mz/JoWood/Gothic II" -tree