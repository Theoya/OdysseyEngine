#include "editor/file_dialog_win32.h"

#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include <cwchar>
#include <vector>
#include <codecvt>
#include <locale>

namespace odyssey::editor {

// Helper: convert UTF-8 std::string to UTF-16 std::wstring (for Win32 APIs).
static std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                          static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                        static_cast<int>(utf8.size()), &wide[0], size_needed);
    return wide;
}

// Helper: convert UTF-16 std::wstring to UTF-8 std::string.
static std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                          static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                        static_cast<int>(wide.size()), &utf8[0], size_needed, nullptr, nullptr);
    return utf8;
}

std::optional<std::filesystem::path> open_scene_dialog() {
    // Initialize COM (required for shell dialogs on some platforms).
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    wchar_t file_name[MAX_PATH] = L"";

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Scene Files (*.scene.xml)\0*.scene.xml\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    bool success = GetOpenFileNameW(&ofn) != 0;
    CoUninitialize();

    if (!success) {
        return std::nullopt;
    }

    return std::filesystem::path(wide_to_utf8(file_name));
}

std::optional<std::filesystem::path> save_scene_dialog(
    const std::filesystem::path& suggested_name) {

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Start with suggested name or empty
    std::wstring initial = utf8_to_wide(suggested_name.string());
    wchar_t file_name[MAX_PATH];
    wcsncpy_s(file_name, MAX_PATH, initial.c_str(), MAX_PATH - 1);

    OPENFILENAMEW sfn = {};
    sfn.lStructSize = sizeof(sfn);
    sfn.hwndOwner = nullptr;
    sfn.lpstrFile = file_name;
    sfn.nMaxFile = MAX_PATH;
    sfn.lpstrFilter = L"Scene Files (*.scene.xml)\0*.scene.xml\0All Files (*.*)\0*.*\0";
    sfn.nFilterIndex = 1;
    sfn.lpstrFileTitle = nullptr;
    sfn.nMaxFileTitle = 0;
    sfn.lpstrDefExt = L"scene.xml";
    sfn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    bool success = GetSaveFileNameW(&sfn) != 0;
    CoUninitialize();

    if (!success) {
        return std::nullopt;
    }

    return std::filesystem::path(wide_to_utf8(file_name));
}

} // namespace odyssey::editor
