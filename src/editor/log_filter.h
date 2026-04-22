#pragma once

// ---------------------------------------------------------------------------
// log_filter.h
// Pure helpers for filtering and transforming the log buffer display.
// Used by LogPanel to apply level filters, search, and collapse duplicates.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

namespace odyssey::editor {

struct LogRow {
    int level = 2;          // spdlog::level: 0=trace, 1=debug, 2=info, 3=warn, 4=err, 5=critical
    std::string msg;
};

struct LogFilterState {
    bool show_info = true;
    bool show_warn = true;
    bool show_error = true;
    std::string search_substr;
    bool collapse_duplicates = false;
};

struct LogDisplayRow {
    LogRow row;
    int count = 1;          // For collapsed duplicates: how many consecutive identical rows
};

// Apply the filter state to the input rows, returning the display list.
// Pure: no I/O, deterministic given the same input and state.
std::vector<LogDisplayRow> apply_log_filter(
    const std::vector<LogRow>& input,
    const LogFilterState& state);

// Count the number of rows by level (ignoring filters; just raw counts).
struct LogCounts {
    int info = 0;
    int warn = 0;
    int error = 0;
};

LogCounts count_by_level(const std::vector<LogRow>& input);

} // namespace odyssey::editor
