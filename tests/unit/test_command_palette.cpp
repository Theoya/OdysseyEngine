#include "editor/command_palette.h"
#include <gtest/gtest.h>

using namespace odyssey::editor;

// Test fuzzy_score with empty query
TEST(CommandPaletteTest, FuzzyScoreEmptyQueryReturnsPositive) {
    int score = fuzzy_score("", "File: New Scene");
    EXPECT_GT(score, 0);
    EXPECT_EQ(score, 1);  // Empty query returns base score of 1
}

// Test fuzzy_score exact match beats substring
TEST(CommandPaletteTest, FuzzyScoreExactMatchBeatsSubstring) {
    int exact = fuzzy_score("new", "New");      // Capital N
    int substring = fuzzy_score("new", "rename");  // "new" is in "rename" but not at start
    EXPECT_GT(exact, substring);
}

// Test fuzzy_score substring match
TEST(CommandPaletteTest, FuzzyScoreSubstringReturnsPositive) {
    int score = fuzzy_score("file", "File: Open");
    EXPECT_GT(score, 0);
}

// Test fuzzy_score no match returns zero
TEST(CommandPaletteTest, FuzzyScoreNoMatchReturnsZero) {
    int score = fuzzy_score("xyz", "File: New Scene");
    EXPECT_EQ(score, 0);
}

// Test fuzzy_score case insensitive
TEST(CommandPaletteTest, FuzzyScoreCaseInsensitive) {
    int lower = fuzzy_score("file", "File: New");
    int upper = fuzzy_score("FILE", "file: new");
    EXPECT_GT(lower, 0);
    EXPECT_GT(upper, 0);
    // Both should match successfully (not testing exact scores, just matching)
}

// Test filter_commands with empty query returns all items
TEST(CommandPaletteTest, FilterCommandsEmptyQueryReturnsAll) {
    CommandRegistry reg;
    reg.items.push_back({"file.new", "File: New", "Ctrl+N", [](EditorState&) {}});
    reg.items.push_back({"file.save", "File: Save", "Ctrl+S", [](EditorState&) {}});
    reg.items.push_back({"edit.undo", "Edit: Undo", "Ctrl+Z", [](EditorState&) {}});

    auto results = filter_commands(reg.items, "");
    EXPECT_EQ(results.size(), 3);
}

// Test filter_commands sorted by score descending
TEST(CommandPaletteTest, FilterCommandsSortedByScoreDescending) {
    CommandRegistry reg;
    reg.items.push_back({"file.new", "File: New Scene", "Ctrl+N", [](EditorState&) {}});
    reg.items.push_back({"edit.undo", "Edit: Undo Action", "Ctrl+Z", [](EditorState&) {}});
    reg.items.push_back({"file.save", "File: Save", "Ctrl+S", [](EditorState&) {}});

    auto results = filter_commands(reg.items, "file");
    EXPECT_GE(results.size(), 2);  // At least "File: New" and "File: Save"

    // First result should be one of the "File:" items
    EXPECT_NE(results[0]->label.find("File:"), std::string::npos);
}

// Test filter_commands with no matching items
TEST(CommandPaletteTest, FilterCommandsNoMatches) {
    CommandRegistry reg;
    reg.items.push_back({"file.new", "File: New Scene", "Ctrl+N", [](EditorState&) {}});
    reg.items.push_back({"file.save", "File: Save", "Ctrl+S", [](EditorState&) {}});

    auto results = filter_commands(reg.items, "xyz");
    EXPECT_EQ(results.size(), 0);
}

// Test register_builtin_commands populates registry
TEST(CommandPaletteTest, RegisterBuiltinCommandsPopulatesRegistry) {
    CommandRegistry reg;
    register_builtin_commands(reg);
    EXPECT_GT(reg.items.size(), 10);  // Should have many commands

    // Check that some standard commands exist
    bool has_save = false;
    bool has_undo = false;
    bool has_play = false;
    for (const auto& item : reg.items) {
        if (item.id == "file.save") has_save = true;
        if (item.id == "edit.undo") has_undo = true;
        if (item.id == "mode.play") has_play = true;
    }
    EXPECT_TRUE(has_save);
    EXPECT_TRUE(has_undo);
    EXPECT_TRUE(has_play);
}
