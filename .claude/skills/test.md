# /test

Run OdysseyEngine tests (unit tests, pipeline tests, or all).

## Usage
`/test [unit|pipeline|all] [--filter <pattern>]`

## Arguments
- `unit`: Run only unit tests (no GPU required)
- `pipeline`: Run only pipeline/GPU tests (requires Vulkan-capable GPU)
- `all`: Run all tests (default if no argument given)
- `--filter <pattern>`: GTest filter pattern to run specific tests (e.g., `BehaviorCompiler.*`)

## Environment
- **VCToolsVersion** must be set to `14.42.34433`
- **Build directory**: `T:/OdysseyEngine/build`
- Tests must be built before running. If test executables are missing, build first.

## Test Executables
- **Unit tests**: `T:/OdysseyEngine/build/Release/odyssey_tests_unit.exe`
- **Pipeline tests**: `T:/OdysseyEngine/build/Release/odyssey_tests_pipeline.exe`

## Test Source Locations
- Unit tests: `T:/OdysseyEngine/tests/unit/` (test_behavior_compiler.cpp, test_buffer_layout.cpp, test_dispatch_config.cpp, test_protocol.cpp, test_script_result.cpp)
- Pipeline tests: `T:/OdysseyEngine/tests/pipeline/` (test_nadir_pipeline.cpp)

## Steps

### 1. Ensure tests are built
```bash
export VCToolsVersion=14.42.34433
cmake --build T:/OdysseyEngine/build --config Release --target odyssey_tests_unit odyssey_tests_pipeline
```

### 2a. Run via ctest (preferred for running categories)
```bash
# Unit tests only
cd T:/OdysseyEngine/build && ctest -C Release -R unit --output-on-failure

# Pipeline tests only
cd T:/OdysseyEngine/build && ctest -C Release -R pipeline --output-on-failure

# All tests
cd T:/OdysseyEngine/build && ctest -C Release --output-on-failure
```

### 2b. Run executables directly (for --filter or verbose output)
```bash
# Unit tests with filter
cd T:/OdysseyEngine/build && ./Release/odyssey_tests_unit.exe --gtest_filter="BehaviorCompiler.*"

# Pipeline tests with filter
cd T:/OdysseyEngine/build && ./Release/odyssey_tests_pipeline.exe --gtest_filter="NadirPipeline.*"

# Verbose output
cd T:/OdysseyEngine/build && ./Release/odyssey_tests_unit.exe --gtest_print_time=1
```

### 3. Report results
- Total tests run, passed, failed
- For failures: show the test name, assertion, and relevant context
- If pipeline tests fail with Vulkan errors, note that a Vulkan-capable GPU is required

## Common Issues

### Test executable not found
Build the project first with `/build`. Test executables are built alongside the main engine.

### Pipeline tests fail with VK_ERROR_INITIALIZATION_FAILED
The machine needs a Vulkan-capable GPU and up-to-date drivers. Pipeline tests cannot run in headless/CI environments without GPU support.

### Tests pass locally but fail in CI
Check `engine.xml` `<testing>` section for `gpu_test_device` and `pipeline_test_timeout_ms` settings.
