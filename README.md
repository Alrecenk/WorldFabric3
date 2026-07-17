# Narball
## Dev Env Setup
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