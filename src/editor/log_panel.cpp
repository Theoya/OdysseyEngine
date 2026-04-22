#include "editor/log_panel.h"
#include "editor/editor.h"
#include "editor/log_filter.h"
#include "editor/mode_enum.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>

#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <string>

namespace odyssey::editor {

// ---------------------------------------------------------------------------
// LogRingBuffer
// ---------------------------------------------------------------------------

void LogRingBuffer::push(LogEntry entry) {
    if (size_ < CAPACITY) {
        buf_[(head_ + size_) % CAPACITY] = std::move(entry);
        ++size_;
    } else {
        // Overwrite oldest
        buf_[head_] = std::move(entry);
        head_ = (head_ + 1) % CAPACITY;
    }
}

const LogEntry& LogRingBuffer::at(size_t logical_index) const {
    return buf_[(head_ + logical_index) % CAPACITY];
}

// ---------------------------------------------------------------------------
// EditorLogSink
// ---------------------------------------------------------------------------

void EditorLogSink::sink_it_(const spdlog::details::log_msg& msg) {
    // Format message to a string
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);

    LogEntry entry;
    entry.level = static_cast<int>(msg.level);
    entry.text.assign(formatted.data(), formatted.size());
    // Trim trailing newline for ImGui display
    while (!entry.text.empty() &&
           (entry.text.back() == '\n' || entry.text.back() == '\r')) {
        entry.text.pop_back();
    }
    buffer_.push(std::move(entry));
}

// ---------------------------------------------------------------------------
// LogPanel
// ---------------------------------------------------------------------------

LogPanel::LogPanel() {
    sink_ = std::make_shared<EditorLogSink>();
    sink_->set_pattern("[%H:%M:%S.%e] [%l] %v");
}

void LogPanel::install_sink() {
    auto logger = spdlog::default_logger();
    logger->sinks().push_back(sink_);
}

static ImU32 color_for_level(int level) {
    // spdlog::level: trace=0, debug=1, info=2, warn=3, err=4, critical=5
    switch (level) {
        case 0: return IM_COL32(130, 130, 150, 255); // trace
        case 1: return IM_COL32(160, 170, 210, 255); // debug
        case 2: return IM_COL32(220, 220, 230, 255); // info
        case 3: return IM_COL32(240, 200,  80, 255); // warn
        case 4: return IM_COL32(240, 100,  90, 255); // err
        case 5: return IM_COL32(255,  60, 150, 255); // critical
        default: return IM_COL32(220, 220, 230, 255);
    }
}

void LogPanel::draw(EditorState& state) {
    if (!visible_) return;

    if (!ImGui::Begin(name_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }

    // --- Clear-on-Play edge trigger ---
    if (clear_on_play_ && state.mode == Mode::Play && last_mode_ != Mode::Play) {
        sink_->buffer().clear();
    }
    last_mode_ = state.mode;

    // --- Control row 1: Clear, Auto-scroll, Clear-on-Play ---
    if (ImGui::Button("Clear")) {
        sink_->buffer().clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    ImGui::SameLine();
    ImGui::Checkbox("Clear on Play", &clear_on_play_);
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu)", sink_->buffer().size());

    // --- Level filters + counts ---
    const auto& rb = sink_->buffer();
    std::vector<LogRow> rows;
    for (size_t i = 0; i < rb.size(); ++i) {
        const auto& e = rb.at(i);
        rows.push_back(LogRow{e.level, e.text});
    }
    auto counts = count_by_level(rows);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Checkbox("Info", &show_info_);
    ImGui::SameLine();
    ImGui::TextDisabled("(%d)", counts.info);
    ImGui::SameLine();
    ImGui::Checkbox("Warn", &show_warn_);
    ImGui::SameLine();
    ImGui::TextDisabled("(%d)", counts.warn);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &show_error_);
    ImGui::SameLine();
    ImGui::TextDisabled("(%d)", counts.error);

    // --- Search + Collapse + Export ---
    ImGui::InputText("##search", search_buf_, sizeof(search_buf_));
    ImGui::SameLine();
    ImGui::Checkbox("Collapse dups", &collapse_duplicates_);
    ImGui::SameLine();
    if (ImGui::SmallButton("Export...")) {
        // Win32 GetSaveFileNameW dialog
        OPENFILENAMEW ofn = {};
        wchar_t szFile[260] = L"";
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
        ofn.lpstrFilter = L"Log Files (*.log)\0*.log\0All Files (*.*)\0*.*\0";
        ofn.lpstrDefExt = L"log";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NONETWORKBUTTON;
        if (GetSaveFileNameW(&ofn)) {
            // Convert wide string to std::string and write the buffer
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, &szFile[0], (int)wcslen(szFile), NULL, 0, NULL, NULL);
            std::string file_path(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, &szFile[0], (int)wcslen(szFile), &file_path[0], size_needed, NULL, NULL);

            std::ofstream out(file_path, std::ios::out);
            if (out.is_open()) {
                for (size_t i = 0; i < rb.size(); ++i) {
                    out << rb.at(i).text << "\n";
                }
                out.close();
                spdlog::info("[editor] log exported to '{}'", file_path);
            }
        }
    }

    ImGui::Separator();

    // --- Apply filters ---
    LogFilterState filter_state;
    filter_state.show_info = show_info_;
    filter_state.show_warn = show_warn_;
    filter_state.show_error = show_error_;
    filter_state.search_substr = std::string(search_buf_);
    filter_state.collapse_duplicates = collapse_duplicates_;

    auto filtered = apply_log_filter(rows, filter_state);

    if (ImGui::BeginChild("##logscroll", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& display_row : filtered) {
            ImGui::PushStyleColor(ImGuiCol_Text, color_for_level(display_row.row.level));
            if (display_row.count > 1) {
                ImGui::TextUnformatted(
                    (display_row.row.msg + " (x" + std::to_string(display_row.count) + ")").c_str());
            } else {
                ImGui::TextUnformatted(display_row.row.msg.c_str());
            }
            ImGui::PopStyleColor();
        }
        if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace odyssey::editor
