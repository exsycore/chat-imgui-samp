# ============================================================
#  CMakeLists.txt — SA-MP ImGui Chat Plugin
# ============================================================
#  Builds a .asi plugin (DLL renamed to .asi) that replaces
#  the default SA-MP chat with a Dear ImGui-based overlay.
#
#  Requirements:
#    - CMake 3.20+
#    - Visual Studio 2022 (or any VS supporting C++20)
#    - Windows SDK with DirectX 9 headers
#
#  Build:
#    cmake -B build -G "Visual Studio 17 2022" -A Win32
#    cmake --build build --config Release
#
#  Output:
#    bin/Release/chatimgui.asi
# ============================================================

cmake_minimum_required(VERSION 3.20)
project("chatimgui" LANGUAGES C CXX VERSION 1.0.0)

# --- Output directories ---
# All build artifacts go to bin/ at the project root.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# --- Third-party libraries (as git submodules) ---
# minhook  — lightweight x86/x64 hooking library
# samp-api — SA-MP reverse-engineered API (header-only style)
add_subdirectory(libs/minhook)
add_subdirectory(libs/samp-api)

# --- Include paths ---
# imgui/       — Dear ImGui core + DX9/Win32 backends
# samp-api/    — SA-MP API headers
# libs/        — tis620.h (Thai TIS-620 <-> UTF-8 converter)
include_directories(libs/imgui)
include_directories(libs/samp-api/include)
include_directories(libs)

# --- Dear ImGui source files ---
# ImGui is compiled directly into the plugin (no separate library).
file(GLOB_RECURSE IMGUI_SOURCE
	"libs/imgui/imgui.cpp"
	"libs/imgui/imgui_demo.cpp"
	"libs/imgui/imgui_draw.cpp"
	"libs/imgui/imgui_tables.cpp"
	"libs/imgui/imgui_widgets.cpp"
	"libs/imgui/backends/imgui_impl_dx9.cpp"
	"libs/imgui/backends/imgui_impl_win32.cpp"
)

# --- Plugin target ---
# Builds a shared library (.dll) renamed to .asi for SA-MP loading.
add_library(${PROJECT_NAME} SHARED)
target_sources(${PROJECT_NAME} PRIVATE
	source/dllmain.cpp
	source/gui.cpp
	${IMGUI_SOURCE}
)

# --- Output name and suffix ---
# SA-MP loads plugins with the .asi extension from its root directory.
set_target_properties(${PROJECT_NAME}
	PROPERTIES
	OUTPUT_NAME ${PROJECT_NAME}
	SUFFIX ".asi"
)

# --- C++ standard and link libraries ---
# C++20 for designated initializers, std::format, etc.
# Links: d3d9 (DirectX 9), minhook (hooking), sampapi (SA-MP API)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_link_libraries(${PROJECT_NAME} PUBLIC d3d9 minhook sampapi)
