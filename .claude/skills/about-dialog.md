# about-dialog

**Help → About** dialog (Batch H).

## Summary
Modal dialog showing OdysseyEngine Editor version, build date, and git hash. Accessible via Help menu or Ctrl+P command palette.

## Capabilities
- **Menu**: Help → About
- **Palette**: Ctrl+P → search "about" → Execute
- **Display**: Version, build date (__DATE__ macro), git hash (stub for now), license line
- **Modal**: Centered, auto-resizing, OK button to close

## Typical Workflow
1. Click Help menu → About
2. Modal dialog appears with engine info
3. User reads version/build info
4. Click OK to dismiss

## Implementation Details
- Function: triggered by `impl_->show_about_dialog = true` flag
- Modal drawn in `Editor::draw_frame()` as ImGui popup
- Text shows:
  - "OdysseyEngine Editor"
  - "Version: dev (Phase 8)"
  - "Build Date: (via __DATE__ macro)"
  - "Git Hash: (unavailable in Batch H)" — full git-hash.h generation deferred
  - "Licensed under MIT"
- Centered via `ImGui::GetMainViewport()->GetCenter()`
- OK button closes with `ImGui::CloseCurrentPopup()`

## Files
- `src/editor/editor.cpp` — ImGui modal in draw_frame() + menu entry in draw_menu_bar()
- Help menu wired to: `impl_->show_about_dialog = true;`

## Future Enhancements (Batch I)
- Generate `git_hash.h` at build time (CMake hook + git rev-parse)
- Add credits link (opens license file in default browser via ShellExecuteW)
- Show available subsystems + their version numbers

## Notes
- __DATE__ is a built-in compiler macro (format: "Mmm dd yyyy")
- Batch I will add actual git hash via build-time code generation
- Modal is modal: blocks interaction with editor until dismissed
