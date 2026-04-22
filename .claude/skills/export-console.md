# /export-console

Export the current log buffer to a `.log` file via Win32 file dialog.

## Usage

In the log panel, click "Export..." button.

## Dialog

- Opens `GetSaveFileNameW` (Windows file save dialog)
- Filters: `*.log` (Log Files), `*.*` (All Files)
- Default extension: `.log`
- Flags: `OFN_OVERWRITEPROMPT | OFN_NONETWORKBUTTON`

## Export Behavior

1. User selects or types a filename
2. If file exists, OS prompts "Overwrite?"
3. Iterate the ring buffer and write each message as a newline-separated text file
4. Log a message: `"[editor] log exported to '<path>'"`

## Format

One message per line, as displayed (including color codes are stripped, just text).

## Notes

- Uses Win32 APIs (`windows.h`, `commdlg.h`)
- UTF-8 encoding (CP_UTF8 wide-to-multibyte conversion)
- Current filters do NOT affect export (all messages in buffer are written)
- Timestamp and level info are included in the message text
