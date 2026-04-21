#pragma once

// ---------------------------------------------------------------------------
// log_panel.h
// A spdlog sink that records the last N log records in a ring buffer and
// displays them in an ImGui window. Colored by level.
// ---------------------------------------------------------------------------

#include "editor/panel.h"

#include <spdlog/sinks/base_sink.h>

#include <array>
#include <mutex>
#include <string>

namespace odyssey::editor {

struct LogEntry {
    int   level = 2;        // spdlog::level
    std::string text;
};

// A fixed-capacity ring buffer. Pure data — no I/O, safe to unit-test.
class LogRingBuffer {
public:
    static constexpr size_t CAPACITY = 512;

    void push(LogEntry entry);
    size_t size() const { return size_; }
    const LogEntry& at(size_t logical_index) const;  // [0, size) oldest->newest
    void clear() { head_ = 0; size_ = 0; }

private:
    std::array<LogEntry, CAPACITY> buf_{};
    size_t head_ = 0;
    size_t size_ = 0;
};

// spdlog sink. Thread-safe (a mutex guards the ring buffer).
class EditorLogSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    EditorLogSink() = default;
    LogRingBuffer& buffer() { return buffer_; }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override {}

private:
    LogRingBuffer buffer_;
};

class LogPanel : public Panel {
public:
    LogPanel();

    const std::string& name() const override { return name_; }
    void draw(EditorState& state) override;

    // Install this panel's sink on the default spdlog logger.
    void install_sink();

private:
    std::string name_ = "Log";
    std::shared_ptr<EditorLogSink> sink_;
    bool auto_scroll_ = true;
};

} // namespace odyssey::editor
