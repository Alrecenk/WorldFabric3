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

## Visual Studio Dev Env Setup
### Install visual studio
### Install Vulkan, but uncheck all extras
DO NOT install GLM or SDL with Vulkan, this will cause linker conflicts!

If you do have them, you'll need to exclude them from the build somehow while including Vulkan SDK.

If you're not using them for another project you can delete their folders entirely.
### In VS, change dropdown from Debug to Release
### Compile once to create the build folder
Compile error is expected
### Copy all DLLs into release build folder
### Compile, success