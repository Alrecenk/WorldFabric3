# Narball
## Building with CMake
Requires CMake 3.24+ and Ninja. The Vulkan SDK is optional; if it isn't found, the Vulkan headers are downloaded automatically.

GCC (MSYS2 UCRT64, `pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja}`):
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

## Visual Studio
1. Install Visual Studio with the "Desktop development with C++" workload (includes CMake and Ninja).
2. File > Open > Folder and select the repo root.
3. Pick "MSVC x64 Release" (or Debug) from the configuration dropdown, then build and run WorldFabric.exe.

If assets fail to load when debugging, set the working directory to the repo root: in `.vs\launch.vs.json`, add `"currentDir": "${workspaceRoot}"` to the WorldFabric.exe configuration.

## Follow-up work
State as of the move from `WorldFabric.vcxproj` to CMake. Both the `gcc-*` and `x64-*` (MSVC) presets build cleanly and the chess demo runs.

### Needs verification
- **Visual Studio IDE debugging (F5) is untested.** The presets use the Ninja generator, which may ignore the `VS_DEBUGGER_WORKING_DIRECTORY` set in `CMakeLists.txt`. If so, the exe starts in `build\<preset>\` and can't find `./assets`. Either use the `launch.vs.json` workaround above, or make the app locate assets relative to the exe or repo root instead of the current directory.
- **VR in GCC builds is untested on a headset.** See the OpenVR note under compiler compatibility below.
- **Debug configuration changed.** The old Debug|x64 config defined `NDEBUG` and used whole-program optimization; CMake's Debug is a normal debug build, so `assert`s are now active. Restore the old flags in `CMakeLists.txt` if that was intentional.

### GCC/MSVC compatibility rules
The prebuilt SDKs in `lib/` and `dll/` are compiled with MSVC. GCC and MSVC disagree on how C++ member functions return structs by value, so calling such a method on a Steam or OpenVR C++ interface from a GCC build returns garbage or crashes.
- **Steamworks:** `ISteamUser::GetSteamID` and `ISteamMatchmaking::GetLobbyOwner` go through the flat C API in `SteamWorksPlugin.cpp` (`getUserSteamID`, `getLobbyOwner`). Any new Steam call that returns `CSteamID` or another struct by value needs the same treatment (see `steam_api_flat.h`).
- **OpenVR:** `IVRSystem::GetProjectionMatrix` and `GetEyeToHeadTransform` use OpenVR's C function table (`FnTable:IVRSystem_022`) in non-MSVC builds (`OpenXRPlugin.cpp`). The `IVRSystemFnTablePrefix` struct mirrors the start of `VR_IVRSystem_FnTable` in `openvr_capi.h`; recheck it if the OpenVR headers are upgraded. New struct-returning OpenVR calls need the same treatment.
- Calls returning integers, enums, `bool` or pointers, or filling output parameters, are safe with either compiler.

### Cleanup candidates
- **Uninitialized Vulkan handles.** `TriangleModel`'s draw-indirect buffer handles in `engine/header/VulkanPlugin.h` were uninitialized and crashed GCC builds; they're now `VK_NULL_HANDLE`. Many other `Vk*` members in that header have no initializer. They're assigned during init today, but initializing them would prevent similar bugs.
- **Indirect-buffer lifetime.** `TriangleModel::render` destroys the previous indirect draw buffer one update later, "in case it's in use". That doesn't guarantee the GPU has finished with it. Tie destruction to a frame fence instead.
- **Unused libraries.** `CMakeLists.txt` links every `.lib` the old project did, but the exe only imports `SDL3`, `SDL3_ttf`, `OpenAL32`, `openvr_api`, `steam_api64` and `vulkan-1`. `glew32`, `OpenGL32`, `SDL3_image/mixer/net/rtf`, `SDL3_test`, `glew32s` and `sdkencryptedappticket64` could likely be dropped.
- **Unused DLLs.** All of `dll/` is copied next to the exe. `SDL2.dll`, `freeglut.dll`, `glfw3.dll`, `glew32.dll`, `steam_api.dll` (32-bit) and the SDL3 extension DLLs aren't imported. `ucrtbase.dll` shouldn't be redistributed this way; it ships with Windows.
- **Shaders aren't built by CMake.** The committed `.spv` files are used as-is; after editing a shader, rerun `shader/compile_shaders.bat` or `Narball/shader/compile_shaders.bat`. A CMake step using `glslc` from the Vulkan SDK would keep them in sync.
- **Choosing the app means editing code.** `main()` in `source/Main.cpp` calls `exampleMain`; switching to Narball means uncommenting `Narball::main`. A command-line flag or CMake option would avoid that.
- **No CI.** A Windows CI job building both `gcc-release` and `x64-release` would catch the kind of MSVC-only code that broke the GCC build.