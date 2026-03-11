# /build

Build the OdysseyEngine project (configure and compile).

## Usage
`/build [--configure] [--clean]`

## Arguments
- `--configure`: Force a CMake reconfigure before building (otherwise only builds if CMakeCache.txt exists)
- `--clean`: Delete the build directory and reconfigure from scratch

## Environment
- **VCToolsVersion** must be set to `14.42.34433` for all build commands
- **Build directory**: `T:/OdysseyEngine/build`
- **Source directory**: `T:/OdysseyEngine`
- **Config**: Always Release
- **Toolchain**: `-DCMAKE_TOOLCHAIN_FILE=T:/vcpkg/scripts/buildsystems/vcpkg.cmake`

## Steps

### 1. Set the required VC tools version
Every shell command in this skill must be prefixed with:
```bash
export VCToolsVersion=14.42.34433
```

### 2. Clean (if --clean)
```bash
rm -rf T:/OdysseyEngine/build
```

### 3. Configure (if --configure, --clean, or no CMakeCache.txt exists)
```bash
export VCToolsVersion=14.42.34433
cmake -B T:/OdysseyEngine/build -S T:/OdysseyEngine \
  -DCMAKE_TOOLCHAIN_FILE=T:/vcpkg/scripts/buildsystems/vcpkg.cmake
```
If configure fails, check these common issues:
- **vcpkg not found**: Verify `T:/vcpkg/scripts/buildsystems/vcpkg.cmake` exists
- **Vulkan SDK missing**: Verify `VULKAN_SDK` environment variable is set
- **Package not found**: Run `vcpkg install` in `T:/OdysseyEngine` to restore dependencies from `vcpkg.json`

### 4. Build
```bash
export VCToolsVersion=14.42.34433
cmake --build T:/OdysseyEngine/build --config Release
```

### 5. Verify
Confirm the executable was produced:
```bash
ls T:/OdysseyEngine/build/Release/odyssey.exe
```

## Common Errors and Fixes

### VCToolsVersion mismatch
**Symptom**: `cl.exe` not found or wrong MSVC version selected.
**Fix**: Ensure `export VCToolsVersion=14.42.34433` is set before every cmake command.

### Missing vcpkg packages
**Symptom**: `find_package(XXX)` fails during configure.
**Fix**: Run `cd T:/OdysseyEngine && vcpkg install` to install all dependencies from `vcpkg.json`.

### Link errors (unresolved externals)
**Symptom**: LNK2019 or LNK2001 errors.
**Fix**: Check that all source files are included via the GLOB_RECURSE patterns in `CMakeLists.txt`. New `.cpp` files in `src/` subdirectories are picked up automatically, but a reconfigure (`--configure`) may be needed.

### Vulkan SDK not found
**Symptom**: `find_package(Vulkan REQUIRED)` fails.
**Fix**: Install the Vulkan SDK and set `VULKAN_SDK` environment variable. On Windows, this is typically `C:/VulkanSDK/<version>`.

### shaderc not found
**Symptom**: `find_package(unofficial-shaderc)` fails.
**Fix**: This comes from vcpkg. Run `vcpkg install` in the project root.

## Output
Report whether the build succeeded or failed. On failure, include the first error message and suggest a fix.
