#include "editor/log_panel.h"
#include "editor/editor.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>

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

void LogPanel::draw(EditorState& /*state*/) {
    if (!visible_) return;

    if (!ImGui::Begin(name_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear")) {
        sink_->buffer().clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu)", sink_->buffer().size());

    ImGui::Separator();

    if (ImGui::BeginChild("##logscroll", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        const auto& rb = sink_->buffer();
        for (size_t i = 0; i < rb.size(); ++i) {
            const auto& e = rb.at(i);
            ImGui::PushStyleColor(ImGuiCol_Text, color_for_level(e.level));
            ImGui::TextUnformatted(e.text.c_str());
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
