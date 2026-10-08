# World Fabric
## Building with CMake on the command line
Requires CMake 3.24+ and Ninja. The Vulkan SDK is optional; if it isn't found, the Vulkan headers are downloaded automatically.

For windows GCC using MSYS2 UCRT64, `pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja}`:
```
cmake --preset gcc-release
cmake --build --preset gcc-release
```

MSVC (from a "x64 Native Tools" / Developer PowerShell prompt, or open the folder in Visual Studio):
```
cmake --preset x64-release
cmake --build --preset x64-release
```

Run from the repo root so `./assets` and `./Narball` resolve, e.g. `build\gcc-release\WorldFabric.exe`, or `cmake --build --preset gcc-release --target run`. The DLLs from `dll/` are copied next to the exe automatically.

## Shaders cmake target is automatic, runs by deault in regular build
Shaders (`*.vert`, `*.frag` and `*.comp` in `shader/` and `Narball/shader/`) are compiled with `glslc` from the Vulkan SDK. Each `.spv` is written next to its source, because that's where the app loads it from. Only edited shaders are recompiled, and new shader files are picked up automatically. Build just the shaders with `cmake --build --preset gcc-release --target shaders`. If `glslc` isn't found, or you configure with `-DWF_COMPILE_SHADERS=OFF`, the committed `.spv` files are used as-is. The `.spv` files are build outputs, so a CMake `clean` deletes them, and the next build regenerates them.

## Visual Studio
1. Install Visual Studio with the "Desktop development with C++" workload (includes CMake and Ninja).
2. File > Open > Folder and select the repo root.
3. Pick "MSVC x64 Release" (or Debug) from the configuration dropdown, then build and run WorldFabric.exe.

### GCC/MSVC compatibility rules
The prebuilt SDKs in `lib/` and `dll/` are compiled with MSVC. GCC and MSVC disagree on how C++ member functions return structs by value, so calling such a method on a Steam or OpenVR C++ interface from a GCC build returns garbage or crashes.
- **Steamworks:** `ISteamUser::GetSteamID` and `ISteamMatchmaking::GetLobbyOwner` go through the flat C API in `SteamWorksPlugin.cpp` (`getUserSteamID`, `getLobbyOwner`). Any new Steam call that returns `CSteamID` or another struct by value needs the same treatment (see `steam_api_flat.h`).
- **OpenVR:** `IVRSystem::GetProjectionMatrix` and `GetEyeToHeadTransform` use OpenVR's C function table (`FnTable:IVRSystem_022`) in non-MSVC builds (`OpenXRPlugin.cpp`). The `IVRSystemFnTablePrefix` struct mirrors the start of `VR_IVRSystem_FnTable` in `openvr_capi.h`; recheck it if the OpenVR headers are upgraded. New struct-returning OpenVR calls need the same treatment.

### Cleanup candidates
- **Uninitialized Vulkan handles.** `TriangleModel`'s draw-indirect buffer handles in `engine/header/VulkanPlugin.h` were uninitialized and crashed GCC builds; they're now `VK_NULL_HANDLE`. Many other `Vk*` members in that header have no initializer. They're assigned during init today, but initializing them would prevent similar bugs.
- **Indirect-buffer lifetime.** `TriangleModel::render` destroys the previous indirect draw buffer one update later, "in case it's in use". That doesn't guarantee the GPU has finished with it. Tie destruction to a frame fence instead.
- **Unused libraries.** `CMakeLists.txt` links every `.lib` the old project did, but the exe only imports `SDL3`, `SDL3_ttf`, `OpenAL32`, `openvr_api`, `steam_api64` and `vulkan-1`. `glew32`, `OpenGL32`, `SDL3_image/mixer/net/rtf`, `SDL3_test`, `glew32s` and `sdkencryptedappticket64` could likely be dropped.
- **Unused DLLs.** All of `dll/` is copied next to the exe. `SDL2.dll`, `freeglut.dll`, `glfw3.dll`, `glew32.dll`, `steam_api.dll` (32-bit) and the SDL3 extension DLLs aren't imported. `ucrtbase.dll` shouldn't be redistributed this way; it ships with Windows.
