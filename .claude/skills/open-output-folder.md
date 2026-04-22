# /open-output-folder

Open the build output folder in Windows Explorer.

## Usage
`/open-output-folder`

## Description
Programmatically clicks the "Open Output Folder" button in the Build Settings panel, using `ShellExecuteW(nullptr, L"explore", output_dir, ...)` to open the distribution folder in Explorer.

No return value; just opens Explorer synchronously.

## Steps

### 1. Ensure Build Settings panel is visible
If hidden, use `/build-settings` to show it.

### 2. Verify output directory
The "Output Dir" field shows the current distribution path (default: `../dist`). Adjust it if needed.

### 3. Open folder
This skill invokes the "Open Output Folder" button, which calls `ShellExecuteW` to open the folder in Explorer.

### 4. Inspect distribution
Once Explorer opens, you can browse the copied binaries, scenes, shaders, schemas, and engine assets that were staged post-build.

## Related
- `/build-settings` — configure build target and output directory
- `/build-game` — start the async build
