# /show-in-explorer

Open the selected asset in Windows File Explorer (Win32 ShellExecuteW).

## Usage

Right-click asset → "Show in Explorer"

## Implementation

```cpp
#include <windows.h>
#include <shellapi.h>

// Win32: open folder and select the file
ShellExecuteW(nullptr, L"open", folder_path.c_str(), nullptr, nullptr, SW_SHOW);
```

## Behavior

- Opens the containing folder in Explorer
- If supported, highlights/selects the asset file
- Equivalent to right-click → "Open folder location" in other tools

## Notes

- Windows-only (uses `ShellExecuteW`)
- Absolute paths required (convert relative if needed)
- Wide string (wchar_t) for international path support
