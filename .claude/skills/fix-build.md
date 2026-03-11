# /fix-build

Diagnose and fix common OdysseyEngine build errors.

## Usage
`/fix-build [<error_text>]`

## Arguments
- `<error_text>`: Optional. Paste or describe the build error. If not provided, run a build and capture the error output.

## Steps

### 1. If no error text provided, attempt a build to capture errors
```bash
export VCToolsVersion=14.42.34433
cmake --build T:/OdysseyEngine/build --config Release 2>&1 | tail -50
```

### 2. Identify the error category and apply the fix

---

### VCToolsVersion Mismatch
**Symptoms**:
- `cl.exe` not found
- Wrong MSVC compiler version selected
- `error MSB8020: The build tools for ... cannot be found`
- `The C compiler identification is unknown`
- `__std_mismatch_4` unresolved external

**Fix**:
Ensure `VCToolsVersion=14.42.34433` is exported before every cmake command:
```bash
export VCToolsVersion=14.42.34433
cmake -B T:/OdysseyEngine/build -S T:/OdysseyEngine \
  -DCMAKE_TOOLCHAIN_FILE=T:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build T:/OdysseyEngine/build --config Release
```

---

### Missing Include / Header Not Found
**Symptoms**:
- `fatal error C1083: Cannot open include file: 'xxx.h'`
- `fatal error: xxx: No such file or directory`

**Diagnosis**:
1. Check if it is a project header: search `T:/OdysseyEngine/src/` for the file
2. Check if it is a vcpkg dependency: search `T:/OdysseyEngine/vcpkg_installed/`
3. Check if it is a Vulkan SDK header: verify `VULKAN_SDK` is set

**Fixes**:
- **Project header**: Include paths are relative to `src/`. Use `#include "core/types.h"` not `#include "types.h"`.
- **vcpkg dependency**: Run `cd T:/OdysseyEngine && vcpkg install` to restore packages.
- **Vulkan header**: Ensure Vulkan SDK is installed and `VULKAN_SDK` environment variable points to it.
- **New subdirectory**: If a new `src/` subdirectory was created, verify the `GLOB_RECURSE` in `CMakeLists.txt` covers it. Add a new glob pattern if needed.

---

### Link Errors (Unresolved Externals)
**Symptoms**:
- `error LNK2019: unresolved external symbol`
- `error LNK2001: unresolved external symbol`

**Diagnosis**:
1. Identify the missing symbol from the error message
2. Search for its definition in the codebase:
```bash
grep -r "symbol_name" T:/OdysseyEngine/src/
```

**Fixes**:
- **Missing .cpp file**: The function is declared in a `.h` but has no `.cpp` implementation. Create the implementation file in the correct `src/` subdirectory.
- **VMA_IMPLEMENTATION undefined**: Ensure `src/vulkan/vma_impl.cpp` exists with `#define VMA_IMPLEMENTATION` before the VMA include.
- **`multiple definition of main`**: `CMakeLists.txt` must exclude `main.cpp` from `APP_SOURCES` (the `list(FILTER APP_SOURCES EXCLUDE REGEX "main\\.cpp$")` line).
- **New source not picked up**: CMake uses `GLOB_RECURSE` but may need a reconfigure:
  ```bash
  export VCToolsVersion=14.42.34433
  cmake -B T:/OdysseyEngine/build -S T:/OdysseyEngine \
    -DCMAKE_TOOLCHAIN_FILE=T:/vcpkg/scripts/buildsystems/vcpkg.cmake
  cmake --build T:/OdysseyEngine/build --config Release
  ```
- **Missing library**: Check `target_link_libraries()` in `CMakeLists.txt`. If a new vcpkg package was added, update both `vcpkg.json` and `CMakeLists.txt`.
- **Winsock functions**: Ensure `ws2_32` is linked (already in CMakeLists.txt for WIN32).
- **`max3`/`max4` NVIDIA intrinsic conflict**: Use `score_max3`/`score_max4` in GLSL instead.

---

### CMake Configure Errors
**Symptoms**:
- `Could not find a package configuration file provided by "XXX"`
- `find_package(XXX) failed`

**Fixes**:
- **vcpkg package**: Add the package to `T:/OdysseyEngine/vcpkg.json` and run `vcpkg install`:
  ```bash
  cd T:/OdysseyEngine && vcpkg install
  ```
- **Vulkan**: Install the Vulkan SDK from https://vulkan.lunarg.com/
- **Toolchain not set**: Ensure the cmake configure command includes:
  ```
  -DCMAKE_TOOLCHAIN_FILE=T:/vcpkg/scripts/buildsystems/vcpkg.cmake
  ```

---

### Compilation Errors (C++ syntax/semantic)
**Symptoms**:
- `error C2065: undeclared identifier`
- `error C2039: 'xxx' is not a member of 'yyy'`
- `error C2664: cannot convert argument`
- `error C7595: use of a deleted function`

**Diagnosis**:
Read the source file mentioned in the error. Check:
1. Is the correct header included?
2. Is the correct namespace used? (Project uses `odyssey::*` namespaces)
3. Is the function signature correct?
4. Is C++20 being used? (Required for `std::format`, concepts, ranges, etc.)

**Fix**: Edit the source file to resolve the issue. The project conventions are:
- `#pragma once` for header guards
- Include paths relative to `src/`
- All functions should be pure where possible
- Namespaces: `odyssey::core`, `odyssey::vulkan`, `odyssey::nadir`, `odyssey::scene`, `odyssey::net`, `odyssey::cli`, `odyssey::debug`, `odyssey::mcp`

---

### Stale Build / Cache Issues
**Symptoms**:
- Build errors that don't match the current code
- Old object files conflicting with new code
- CMake variables stuck at wrong values

**Fix**: Clean rebuild:
```bash
rm -rf T:/OdysseyEngine/build/CMakeCache.txt T:/OdysseyEngine/build/CMakeFiles
export VCToolsVersion=14.42.34433
cmake -B T:/OdysseyEngine/build -S T:/OdysseyEngine \
  -DCMAKE_TOOLCHAIN_FILE=T:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build T:/OdysseyEngine/build --config Release
```

## Output
Report:
1. What error was found
2. What category it falls into
3. What fix was applied
4. Whether the build now succeeds
