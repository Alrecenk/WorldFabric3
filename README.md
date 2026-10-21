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

## Shaders cmake target is automatic, runs by deault on stale shaders
Shaders are compiled with `glslc` from the Vulkan SDK. Each `.spv` is written next to its source,
because that's where the app loads it from. Only edited shaders are recompiled,
and new shader files are picked up automatically.
Build just the shaders with `cmake --build --preset gcc-release --target shaders`.

## Visual Studio
1. Install Visual Studio with the "Desktop development with C++" workload (includes CMake and Ninja).
2. File > Open > Folder and select the repo root.
3. Pick "MSVC x64 Release" (or Debug) from the configuration dropdown, then build and run WorldFabric.exe.

### GCC/MSVC compatibility rules
The prebuilt SDKs in `lib/` and `dll/` are compiled with MSVC. GCC and MSVC disagree on how C++ member functions return structs by value, so calling such a method on a Steam or OpenVR C++ interface from a GCC build returns garbage or crashes.
- **Steamworks:** `ISteamUser::GetSteamID` and `ISteamMatchmaking::GetLobbyOwner` go through the flat C API in `SteamWorksPlugin.cpp` (`getUserSteamID`, `getLobbyOwner`). Any new Steam call that returns `CSteamID` or another struct by value needs the same treatment (see `steam_api_flat.h`).
- **OpenVR:** `IVRSystem::GetProjectionMatrix` and `GetEyeToHeadTransform` use OpenVR's C function table (`FnTable:IVRSystem_022`) in non-MSVC builds (`OpenXRPlugin.cpp`). The `IVRSystemFnTablePrefix` struct mirrors the start of `VR_IVRSystem_FnTable` in `openvr_capi.h`; recheck it if the OpenVR headers are upgraded. New struct-returning OpenVR calls need the same treatment.

### Cleanup candidates
- **Indirect-buffer lifetime.** `TriangleModel::render` destroys the previous indirect draw buffer one update later, "in case it's in use". That doesn't guarantee the GPU has finished with it. Tie destruction to a frame fence instead.
- clang-tidy can be used to automate initializing variables to their respective null types
- Using GCC with `-Wuninitialized -Wmaybe-uninitialized` at `-O2` catches most uninitialized values.`-Wreturn-type` is also good to check that non-void functions' returns exist