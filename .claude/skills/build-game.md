# /build-game

Start an async build from the Build Settings panel.

## Usage
`/build-game`

## Description
Programmatically clicks the Build button in the Build Settings panel, starting an async cmake build process with the current target, config, and output directory.

The build is non-blocking: the panel shows a spinner while running and captures full stdout/stderr in a collapsing header. No return value until build completes.

## Steps

### 1. Ensure Build Settings panel is visible
If hidden, use `/build-settings` to show it.

### 2. Configure targets and scenes
Set up the build configuration: choose target, add scenes to the list, set config and output dir.

### 3. Trigger build
This skill invokes `BuildRunner::run_build()` with the current panel state, spawning a `CreateProcessA` thread for cmake.

### 4. Monitor progress
The panel shows:
- Spinner while build is running
- Exit code and state (Success/Failed/TimedOut) when done
- Full stdout and stderr logs in the collapsing "Build Output" header

### 5. Retrieve result
After build completes, manually inspect the output or programmatically call `BuildRunner::get_result()` to fetch `BuildResult{state, exit_code, stdout_log, stderr_log}`.

## Related
- `/build-settings` — open the panel
- `/open-output-folder` — view the distribution folder
