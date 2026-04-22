#include <gtest/gtest.h>
#include "editor/undo_stack.h"
#include "scene/entity_manager.h"
#include "scene/scene_loader.h"

using namespace odyssey::editor;
using namespace odyssey::scene;

TEST(UndoStack, EmptyInitially) {
    UndoStack stack;
    EXPECT_FALSE(stack.can_undo());
    EXPECT_FALSE(stack.can_redo());
    EXPECT_EQ(stack.undo_count(), 0);
    EXPECT_EQ(stack.redo_count(), 0);
}

TEST(UndoStack, PushAndPeekUndo) {
    UndoStack stack;
    SceneData data;
    EntityManager em;

    UndoEntry entry;
    entry.description = "Create Entity";
    entry.state = data;

    stack.push(std::move(entry));

    EXPECT_TRUE(stack.can_undo());
    EXPECT_FALSE(stack.can_redo());

    const auto* peeked = stack.peek_undo();
    EXPECT_NE(peeked, nullptr);
    EXPECT_EQ(peeked->description, "Create Entity");
}

TEST(UndoStack, PopUndoMovesToRedo) {
    // F2: undo/redo basic and redo after new push is dropped
    UndoStack stack;
    SceneData data;
    EntityManager em;

    UndoEntry entry1;
    entry1.description = "Action1";
    entry1.state = data;
    stack.push(std::move(entry1));

    UndoEntry entry2;
    entry2.description = "Action2";
    entry2.state = data;
    stack.push(std::move(entry2));

    EXPECT_EQ(stack.undo_count(), 2);
    EXPECT_EQ(stack.redo_count(), 0);

    auto popped = stack.pop_undo();
    EXPECT_EQ(popped.description, "Action2");
    EXPECT_EQ(stack.undo_count(), 1);
    EXPECT_EQ(stack.redo_count(), 1);

    auto redo_entry = stack.pop_redo();
    EXPECT_EQ(redo_entry.description, "Action2");
    EXPECT_EQ(stack.undo_count(), 2);
    EXPECT_EQ(stack.redo_count(), 0);
}

TEST(UndoStack, PushDisccardsRedo) {
    // F2: redo after new push is dropped
    UndoStack stack;
    SceneData data;

    UndoEntry e1;
    e1.description = "A";
    e1.state = data;
    stack.push(std::move(e1));

    UndoEntry e2;
    e2.description = "B";
    e2.state = data;
    stack.push(std::move(e2));

    // Undo to get a redo history
    stack.pop_undo();
    EXPECT_EQ(stack.redo_count(), 1);

    // Push new entry — should clear redo
    UndoEntry e3;
    e3.description = "C";
    e3.state = data;
    stack.push(std::move(e3));

    EXPECT_EQ(stack.redo_count(), 0);
    EXPECT_EQ(stack.undo_count(), 2);  // A and C remain
}

TEST(UndoStack, MaxDepthTrimsOldest) {
    // F4: max depth trims oldest
    UndoStack stack;
    SceneData data;

    // Push exactly kMaxDepth + 1 entries
    for (size_t i = 0; i < UndoStack::kMaxDepth + 1; ++i) {
        UndoEntry entry;
        entry.description = "Entry" + std::to_string(i);
        entry.state = data;
        stack.push(std::move(entry));
    }

    EXPECT_EQ(stack.undo_count(), UndoStack::kMaxDepth);

    // Oldest entry should be gone
    auto oldest = stack.pop_undo();
    EXPECT_NE(oldest.description, "Entry0");  // First entry was trimmed
}

TEST(UndoStack, PeekUndoNull) {
    // F5: peek_undo nullptr when empty
    UndoStack stack;
    EXPECT_EQ(stack.peek_undo(), nullptr);
    EXPECT_EQ(stack.peek_redo(), nullptr);
}

TEST(UndoStack, ClearReset) {
    // F6: clear()
    UndoStack stack;
    SceneData data;

    UndoEntry entry;
    entry.description = "Test";
    entry.state = data;
    stack.push(std::move(entry));

    stack.pop_undo();  // Move to redo

    EXPECT_EQ(stack.undo_count(), 0);
    EXPECT_EQ(stack.redo_count(), 1);

    stack.clear();
    EXPECT_EQ(stack.undo_count(), 0);
    EXPECT_EQ(stack.redo_count(), 0);
    EXPECT_FALSE(stack.can_undo());
    EXPECT_FALSE(stack.can_redo());
}

TEST(UndoStack, CountQueries) {
    // F7: undo_count/redo_count
    UndoStack stack;
    SceneData data;

    for (int i = 0; i < 3; ++i) {
        UndoEntry entry;
        entry.description = "E" + std::to_string(i);
        entry.state = data;
        stack.push(std::move(entry));
    }

    EXPECT_EQ(stack.undo_count(), 3);
    EXPECT_EQ(stack.redo_count(), 0);

    stack.pop_undo();
    EXPECT_EQ(stack.undo_count(), 2);
    EXPECT_EQ(stack.redo_count(), 1);

    stack.pop_undo();
    EXPECT_EQ(stack.undo_count(), 1);
    EXPECT_EQ(stack.redo_count(), 2);
}
