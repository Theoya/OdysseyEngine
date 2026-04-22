#include "editor/log_filter.h"

#include <algorithm>
#include <cctype>

namespace odyssey::editor {

// Pure: case-insensitive substring search.
static bool contains_substr_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;

    for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            char h = haystack[i + j];
            char n = needle[j];
            if (h >= 'A' && h <= 'Z') h = static_cast<char>(h - 'A' + 'a');
            if (n >= 'A' && n <= 'Z') n = static_cast<char>(n - 'A' + 'a');
            if (h != n) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// Map spdlog level to level group (info/warn/error).
static bool should_show_level(int level, const LogFilterState& state) {
    // spdlog::level: trace=0, debug=1, info=2, warn=3, err=4, critical=5
    if (level <= 2) return state.show_info;      // trace, debug, info
    if (level == 3) return state.show_warn;      // warn
    if (level >= 4) return state.show_error;     // err, critical
    return state.show_info;
}

std::vector<LogDisplayRow> apply_log_filter(
    const std::vector<LogRow>& input,
    const LogFilterState& state) {

    std::vector<LogDisplayRow> out;

    for (const auto& row : input) {
        // Check level filter
        if (!should_show_level(row.level, state)) {
            continue;
        }

        // Check search filter
        if (!state.search_substr.empty() &&
            !contains_substr_ci(row.msg, state.search_substr)) {
            continue;
        }

        // Row passes filters. Check if we should collapse with the previous row.
        if (state.collapse_duplicates && !out.empty() &&
            out.back().row.level == row.level &&
            out.back().row.msg == row.msg) {
            // Duplicate: increment count
            out.back().count++;
        } else {
            // New row
            out.push_back(LogDisplayRow{row, 1});
        }
    }

    return out;
}

LogCounts count_by_level(const std::vector<LogRow>& input) {
    LogCounts counts;
    for (const auto& row : input) {
        if (row.level <= 2) {
            counts.info++;
        } else if (row.level == 3) {
            counts.warn++;
        } else if (row.level >= 4) {
            counts.error++;
        }
    }
    return counts;
}

} // namespace odyssey::editor
