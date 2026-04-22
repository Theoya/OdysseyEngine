// ---------------------------------------------------------------------------
// test_log_filter.cpp
//
// Unit tests for the pure helpers in src/editor/log_filter.{h,cpp}.
// Covers:
//   - apply_log_filter: level filtering (info/warn/error toggles)
//   - apply_log_filter: search substring (case-insensitive)
//   - apply_log_filter: collapse duplicates (consecutive identical rows)
//   - count_by_level: raw level counts
//   - edge cases: empty input, search with no matches, all filters disabled
// No Vulkan, no ImGui, no networking — fast pure function tests.
// ---------------------------------------------------------------------------

#include "editor/log_filter.h"

#include <gtest/gtest.h>

using namespace odyssey::editor;

namespace {

// Helper: create a LogRow with a given level and message.
LogRow make_row(int level, const std::string& msg) {
    return {level, msg};
}

} // namespace

// ---------------------------------------------------------------------------
// apply_log_filter: Level filtering
// ---------------------------------------------------------------------------

TEST(LogFilter, LevelFilterHidesInfo) {
    std::vector<LogRow> input = {
        make_row(2, "info message"),      // 2 = info
        make_row(3, "warn message"),      // 3 = warn
    };
    LogFilterState state;
    state.show_info = false;
    state.show_warn = true;
    state.show_error = true;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].row.msg, "warn message");
}

TEST(LogFilter, LevelFilterHidesWarn) {
    std::vector<LogRow> input = {
        make_row(2, "info message"),
        make_row(3, "warn message"),
        make_row(4, "error message"),    // 4 = error
    };
    LogFilterState state;
    state.show_info = true;
    state.show_warn = false;
    state.show_error = true;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].row.msg, "info message");
    EXPECT_EQ(result[1].row.msg, "error message");
}

TEST(LogFilter, LevelFilterHidesError) {
    std::vector<LogRow> input = {
        make_row(2, "info message"),
        make_row(4, "error message"),
        make_row(5, "critical message"),  // 5 = critical
    };
    LogFilterState state;
    state.show_info = true;
    state.show_warn = true;
    state.show_error = false;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].row.msg, "info message");
}

// ---------------------------------------------------------------------------
// apply_log_filter: Search substring
// ---------------------------------------------------------------------------

TEST(LogFilter, SearchSubstringMatch) {
    std::vector<LogRow> input = {
        make_row(2, "loaded texture.png"),
        make_row(2, "loaded scene.xml"),
        make_row(3, "failed to parse"),
    };
    LogFilterState state;
    state.search_substr = "loaded";

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].row.msg, "loaded texture.png");
    EXPECT_EQ(result[1].row.msg, "loaded scene.xml");
}

TEST(LogFilter, SearchIsNotCaseSensitive) {
    std::vector<LogRow> input = {
        make_row(2, "Error in shader"),
        make_row(2, "ERROR: something failed"),
        make_row(2, "clean execution"),
    };
    LogFilterState state;
    state.search_substr = "error";

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].row.msg, "Error in shader");
    EXPECT_EQ(result[1].row.msg, "ERROR: something failed");
}

TEST(LogFilter, SearchEmptySubstringShowsAll) {
    std::vector<LogRow> input = {
        make_row(2, "first"),
        make_row(3, "second"),
        make_row(4, "third"),
    };
    LogFilterState state;
    state.search_substr = "";  // empty

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 3);
}

TEST(LogFilter, SearchNoMatches) {
    std::vector<LogRow> input = {
        make_row(2, "foo"),
        make_row(2, "bar"),
    };
    LogFilterState state;
    state.search_substr = "xyz";

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 0);
}

// ---------------------------------------------------------------------------
// apply_log_filter: Collapse duplicates
// ---------------------------------------------------------------------------

TEST(LogFilter, CollapseDuplicatesGroupsConsecutiveIdentical) {
    std::vector<LogRow> input = {
        make_row(2, "connecting..."),
        make_row(2, "connecting..."),
        make_row(2, "connecting..."),
        make_row(3, "timeout"),
    };
    LogFilterState state;
    state.collapse_duplicates = true;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].row.msg, "connecting...");
    EXPECT_EQ(result[0].count, 3);
    EXPECT_EQ(result[1].row.msg, "timeout");
    EXPECT_EQ(result[1].count, 1);
}

TEST(LogFilter, CollapseDuplicatesDoesNotGroupNonConsecutive) {
    std::vector<LogRow> input = {
        make_row(2, "connecting..."),
        make_row(3, "timeout"),
        make_row(2, "connecting..."),
    };
    LogFilterState state;
    state.collapse_duplicates = true;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].count, 1);
    EXPECT_EQ(result[1].count, 1);
    EXPECT_EQ(result[2].count, 1);
}

TEST(LogFilter, CollapseDuplicatesIsSensitiveToLevel) {
    std::vector<LogRow> input = {
        make_row(2, "same msg"),
        make_row(3, "same msg"),  // Different level, so not a duplicate
    };
    LogFilterState state;
    state.collapse_duplicates = true;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].count, 1);
    EXPECT_EQ(result[1].count, 1);
}

TEST(LogFilter, CollapseDuplicatesDisabledShowsAll) {
    std::vector<LogRow> input = {
        make_row(2, "msg"),
        make_row(2, "msg"),
        make_row(2, "msg"),
    };
    LogFilterState state;
    state.collapse_duplicates = false;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].count, 1);
    EXPECT_EQ(result[1].count, 1);
    EXPECT_EQ(result[2].count, 1);
}

// ---------------------------------------------------------------------------
// apply_log_filter: Combined filters
// ---------------------------------------------------------------------------

TEST(LogFilter, CombinedLevelAndSearch) {
    std::vector<LogRow> input = {
        make_row(2, "loaded mesh.xml"),
        make_row(2, "created asset"),
        make_row(3, "loading timed out"),
        make_row(4, "could not load shader"),
    };
    LogFilterState state;
    state.show_info = true;
    state.show_warn = false;
    state.show_error = true;
    state.search_substr = "load";

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].row.msg, "loaded mesh.xml");
    EXPECT_EQ(result[1].row.msg, "could not load shader");
}

// ---------------------------------------------------------------------------
// apply_log_filter: Edge cases
// ---------------------------------------------------------------------------

TEST(LogFilter, EmptyInputReturnsEmpty) {
    std::vector<LogRow> input;
    LogFilterState state;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 0);
}

TEST(LogFilter, AllFiltersDisabledReturnsEmpty) {
    std::vector<LogRow> input = {
        make_row(2, "msg"),
    };
    LogFilterState state;
    state.show_info = false;
    state.show_warn = false;
    state.show_error = false;

    auto result = apply_log_filter(input, state);
    EXPECT_EQ(result.size(), 0);
}

// ---------------------------------------------------------------------------
// count_by_level
// ---------------------------------------------------------------------------

TEST(LogCounts, CountsAllLevels) {
    std::vector<LogRow> input = {
        make_row(0, "trace"),
        make_row(1, "debug"),
        make_row(2, "info"),
        make_row(2, "info 2"),
        make_row(3, "warn"),
        make_row(4, "error"),
        make_row(5, "critical"),
    };

    auto counts = count_by_level(input);
    EXPECT_EQ(counts.info, 4);    // trace(1) + debug(1) + info(2) = 4
    EXPECT_EQ(counts.warn, 1);    // warn(1)
    EXPECT_EQ(counts.error, 2);   // error(1) + critical(1)
}

TEST(LogCounts, EmptyInput) {
    std::vector<LogRow> input;
    auto counts = count_by_level(input);
    EXPECT_EQ(counts.info, 0);
    EXPECT_EQ(counts.warn, 0);
    EXPECT_EQ(counts.error, 0);
}

TEST(LogCounts, OnlyInfo) {
    std::vector<LogRow> input = {
        make_row(2, "msg1"),
        make_row(2, "msg2"),
    };
    auto counts = count_by_level(input);
    EXPECT_EQ(counts.info, 2);
    EXPECT_EQ(counts.warn, 0);
    EXPECT_EQ(counts.error, 0);
}
