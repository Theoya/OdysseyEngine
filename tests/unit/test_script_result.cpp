#include <gtest/gtest.h>
#include "scripting/script_result.h"
#include "scripting/script_context.h"
#include "scripting/script_runner.h"
#include "scripting/script.h"

using namespace odyssey;
using namespace odyssey::scripting;

// ---------------------------------------------------------------------------
// ScriptResult — fluent API
// ---------------------------------------------------------------------------

TEST(ScriptResult, StartsEmpty) {
    ScriptResult result;
    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.mutation_count(), 0u);
}

TEST(ScriptResult, SetStateAddsOneMutation) {
    ScriptResult result;
    result.set("quest.started", std::any(true));
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.mutation_count(), 1u);
    EXPECT_TRUE(std::holds_alternative<SetStateMutation>(result.mutations()[0]));
    const auto& m = std::get<SetStateMutation>(result.mutations()[0]);
    EXPECT_EQ(m.key, "quest.started");
    EXPECT_TRUE(std::any_cast<bool>(m.value));
}

TEST(ScriptResult, SpawnEntity) {
    ScriptResult result;
    result.spawn_entity("goblin", vec3{1.f, 2.f, 3.f}, "goblin_01");
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<SpawnEntityMutation>(result.mutations()[0]);
    EXPECT_EQ(m.prefab, "goblin");
    EXPECT_FLOAT_EQ(m.position.x, 1.f);
    EXPECT_FLOAT_EQ(m.position.y, 2.f);
    EXPECT_FLOAT_EQ(m.position.z, 3.f);
    EXPECT_EQ(m.name, "goblin_01");
}

TEST(ScriptResult, DestroyEntity) {
    ScriptResult result;
    result.destroy_entity(42);
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<DestroyEntityMutation>(result.mutations()[0]);
    EXPECT_EQ(m.entity_id, 42u);
}

TEST(ScriptResult, DamageWithSource) {
    ScriptResult result;
    result.damage(10, 25.5f, 5);
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<DamageEntityMutation>(result.mutations()[0]);
    EXPECT_EQ(m.target, 10u);
    EXPECT_FLOAT_EQ(m.amount, 25.5f);
    EXPECT_EQ(m.source, 5u);
}

TEST(ScriptResult, DamageWithoutSource) {
    ScriptResult result;
    result.damage(10, 50.f);
    const auto& m = std::get<DamageEntityMutation>(result.mutations()[0]);
    EXPECT_EQ(m.source, INVALID_ENTITY);
}

TEST(ScriptResult, HealEntity) {
    ScriptResult result;
    result.heal(7, 30.f);
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<HealEntityMutation>(result.mutations()[0]);
    EXPECT_EQ(m.target, 7u);
    EXPECT_FLOAT_EQ(m.amount, 30.f);
}

TEST(ScriptResult, AddItem) {
    ScriptResult result;
    result.add_item(1, "health_potion", 3);
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<AddItemMutation>(result.mutations()[0]);
    EXPECT_EQ(m.entity_id, 1u);
    EXPECT_EQ(m.item, "health_potion");
    EXPECT_EQ(m.count, 3u);
}

TEST(ScriptResult, RemoveItem) {
    ScriptResult result;
    result.remove_item(1, "gold_coin", 10);
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<RemoveItemMutation>(result.mutations()[0]);
    EXPECT_EQ(m.entity_id, 1u);
    EXPECT_EQ(m.item, "gold_coin");
    EXPECT_EQ(m.count, 10u);
}

TEST(ScriptResult, TriggerDialogue) {
    ScriptResult result;
    result.trigger_dialogue("intro_conversation", 99);
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<TriggerDialogueMutation>(result.mutations()[0]);
    EXPECT_EQ(m.dialogue_id, "intro_conversation");
    EXPECT_EQ(m.speaker, 99u);
}

TEST(ScriptResult, PlaySound) {
    ScriptResult result;
    result.play_sound("explosion", vec3{5.f, 0.f, 0.f}, 0.8f);
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<PlaySoundMutation>(result.mutations()[0]);
    EXPECT_EQ(m.sound_id, "explosion");
    EXPECT_FLOAT_EQ(m.position.x, 5.f);
    EXPECT_FLOAT_EQ(m.volume, 0.8f);
}

TEST(ScriptResult, SetTransform) {
    Transform t;
    t.position = vec3{10.f, 20.f, 30.f};
    t.rotation = quat{1.f, 0.f, 0.f, 0.f};
    t.scale = vec3{2.f};

    ScriptResult result;
    result.set_transform(5, t);
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<SetTransformMutation>(result.mutations()[0]);
    EXPECT_EQ(m.entity_id, 5u);
    EXPECT_FLOAT_EQ(m.transform.position.x, 10.f);
    EXPECT_FLOAT_EQ(m.transform.scale.x, 2.f);
}

TEST(ScriptResult, LogMessage) {
    ScriptResult result;
    result.log("something happened", "warn");
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& m = std::get<LogMessageMutation>(result.mutations()[0]);
    EXPECT_EQ(m.message, "something happened");
    EXPECT_EQ(m.level, "warn");
}

TEST(ScriptResult, LogDefaultLevel) {
    ScriptResult result;
    result.log("info message");
    const auto& m = std::get<LogMessageMutation>(result.mutations()[0]);
    EXPECT_EQ(m.level, "info");
}

TEST(ScriptResult, FluentChaining) {
    ScriptResult result;
    result
        .set("counter", std::any(42))
        .damage(1, 10.f)
        .heal(2, 5.f)
        .log("chained");

    EXPECT_EQ(result.mutation_count(), 4u);
    EXPECT_TRUE(std::holds_alternative<SetStateMutation>(result.mutations()[0]));
    EXPECT_TRUE(std::holds_alternative<DamageEntityMutation>(result.mutations()[1]));
    EXPECT_TRUE(std::holds_alternative<HealEntityMutation>(result.mutations()[2]));
    EXPECT_TRUE(std::holds_alternative<LogMessageMutation>(result.mutations()[3]));
}

TEST(ScriptResult, Clear) {
    ScriptResult result;
    result.damage(1, 10.f).heal(2, 5.f);
    EXPECT_EQ(result.mutation_count(), 2u);
    result.clear();
    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.mutation_count(), 0u);
}

// ---------------------------------------------------------------------------
// ScriptResult — merge
// ---------------------------------------------------------------------------

TEST(ScriptResult, MergeCombinesMutations) {
    ScriptResult a;
    a.damage(1, 10.f);
    a.heal(2, 5.f);

    ScriptResult b;
    b.spawn_entity("orc", vec3{0.f});
    b.log("merged");

    a.merge(b);
    EXPECT_EQ(a.mutation_count(), 4u);
    EXPECT_TRUE(std::holds_alternative<DamageEntityMutation>(a.mutations()[0]));
    EXPECT_TRUE(std::holds_alternative<HealEntityMutation>(a.mutations()[1]));
    EXPECT_TRUE(std::holds_alternative<SpawnEntityMutation>(a.mutations()[2]));
    EXPECT_TRUE(std::holds_alternative<LogMessageMutation>(a.mutations()[3]));
}

TEST(ScriptResult, MergeEmptyIntoEmpty) {
    ScriptResult a;
    ScriptResult b;
    a.merge(b);
    EXPECT_TRUE(a.empty());
}

TEST(ScriptResult, MergeEmptyIntoNonEmpty) {
    ScriptResult a;
    a.damage(1, 10.f);
    ScriptResult b;
    a.merge(b);
    EXPECT_EQ(a.mutation_count(), 1u);
}

// ---------------------------------------------------------------------------
// ScriptContext — state store
// ---------------------------------------------------------------------------

TEST(ScriptContext, GetSetState) {
    ScriptContext ctx;
    ctx.set_state("quest.stage", std::any(3));
    EXPECT_EQ(ctx.get<int>("quest.stage"), 3);
}

TEST(ScriptContext, GetMissingKeyReturnsDefault) {
    ScriptContext ctx;
    EXPECT_EQ(ctx.get<int>("missing"), 0);
    EXPECT_EQ(ctx.get<std::string>("missing"), std::string{});
}

TEST(ScriptContext, GetWithExplicitDefault) {
    ScriptContext ctx;
    EXPECT_EQ(ctx.get<int>("missing", 42), 42);
    EXPECT_EQ(ctx.get<std::string>("missing", "fallback"), "fallback");
}

TEST(ScriptContext, GetWrongTypeFallsBack) {
    ScriptContext ctx;
    ctx.set_state("key", std::any(std::string("hello")));
    // Requesting int from a string value should return default
    EXPECT_EQ(ctx.get<int>("key"), 0);
    EXPECT_EQ(ctx.get<int>("key", -1), -1);
}

TEST(ScriptContext, HasKey) {
    ScriptContext ctx;
    EXPECT_FALSE(ctx.has_key("foo"));
    ctx.set_state("foo", std::any(true));
    EXPECT_TRUE(ctx.has_key("foo"));
}

// ---------------------------------------------------------------------------
// ScriptContext — entity queries
// ---------------------------------------------------------------------------

TEST(ScriptContext, EntityPosition) {
    ScriptContext ctx;
    ctx.add_entity_position(1, vec3{10.f, 20.f, 30.f});
    vec3 pos = ctx.get_position(1);
    EXPECT_FLOAT_EQ(pos.x, 10.f);
    EXPECT_FLOAT_EQ(pos.y, 20.f);
    EXPECT_FLOAT_EQ(pos.z, 30.f);
}

TEST(ScriptContext, UnknownEntityPositionReturnsZero) {
    ScriptContext ctx;
    vec3 pos = ctx.get_position(999);
    EXPECT_FLOAT_EQ(pos.x, 0.f);
    EXPECT_FLOAT_EQ(pos.y, 0.f);
    EXPECT_FLOAT_EQ(pos.z, 0.f);
}

TEST(ScriptContext, EntityHealth) {
    ScriptContext ctx;
    ctx.add_entity_health(1, 75.f, 100.f);
    EXPECT_FLOAT_EQ(ctx.get_health(1), 75.f);
    EXPECT_TRUE(ctx.is_alive(1));
}

TEST(ScriptContext, DeadEntity) {
    ScriptContext ctx;
    ctx.add_entity_health(1, 0.f, 100.f);
    EXPECT_FALSE(ctx.is_alive(1));
}

TEST(ScriptContext, UnknownEntityNotAlive) {
    ScriptContext ctx;
    EXPECT_FALSE(ctx.is_alive(999));
}

TEST(ScriptContext, Distance) {
    ScriptContext ctx;
    ctx.add_entity_position(1, vec3{0.f, 0.f, 0.f});
    ctx.add_entity_position(2, vec3{3.f, 4.f, 0.f});
    EXPECT_FLOAT_EQ(ctx.get_distance(1, 2), 5.f);
}

TEST(ScriptContext, EntitiesInRadius) {
    ScriptContext ctx;
    ctx.add_entity_position(1, vec3{0.f, 0.f, 0.f});
    ctx.add_entity_position(2, vec3{3.f, 0.f, 0.f});
    ctx.add_entity_position(3, vec3{10.f, 0.f, 0.f});

    auto nearby = ctx.entities_in_radius(vec3{0.f}, 5.f);
    EXPECT_EQ(nearby.size(), 2u);
    // Both entity 1 and 2 should be in radius
    EXPECT_TRUE(std::find(nearby.begin(), nearby.end(), 1u) != nearby.end());
    EXPECT_TRUE(std::find(nearby.begin(), nearby.end(), 2u) != nearby.end());
    EXPECT_TRUE(std::find(nearby.begin(), nearby.end(), 3u) == nearby.end());
}

TEST(ScriptContext, FindEntityByName) {
    ScriptContext ctx;
    ctx.add_entity_name(42, "player");
    EXPECT_EQ(ctx.find_entity("player"), 42u);
    EXPECT_EQ(ctx.find_entity("nonexistent"), INVALID_ENTITY);
}

TEST(ScriptContext, HasItem) {
    ScriptContext ctx;
    ctx.add_item(1, "sword");
    ctx.add_item(1, "shield");
    EXPECT_TRUE(ctx.has_item(1, "sword"));
    EXPECT_TRUE(ctx.has_item(1, "shield"));
    EXPECT_FALSE(ctx.has_item(1, "bow"));
    EXPECT_FALSE(ctx.has_item(2, "sword"));
}

// ---------------------------------------------------------------------------
// ScriptContext — input state
// ---------------------------------------------------------------------------

TEST(ScriptContext, KeyState) {
    ScriptContext ctx;
    ctx.set_key_state("space", true, true);
    ctx.set_key_state("shift", true, false);
    EXPECT_TRUE(ctx.is_key_pressed("space"));
    EXPECT_TRUE(ctx.is_key_just_pressed("space"));
    EXPECT_TRUE(ctx.is_key_pressed("shift"));
    EXPECT_FALSE(ctx.is_key_just_pressed("shift"));
    EXPECT_FALSE(ctx.is_key_pressed("ctrl"));
    EXPECT_FALSE(ctx.is_key_just_pressed("ctrl"));
}

// ---------------------------------------------------------------------------
// ScriptContext — clear
// ---------------------------------------------------------------------------

TEST(ScriptContext, ClearResetsEverything) {
    ScriptContext ctx;
    ctx.player_id = 1;
    ctx.delta_time = 0.016f;
    ctx.total_time = 10.f;
    ctx.frame_number = 600;
    ctx.set_state("key", std::any(true));
    ctx.add_entity_position(1, vec3{1.f});
    ctx.add_entity_health(1, 50.f, 100.f);
    ctx.add_entity_name(1, "test");
    ctx.add_item(1, "item");
    ctx.set_key_state("w", true, false);

    ctx.clear();

    EXPECT_EQ(ctx.player_id, INVALID_ENTITY);
    EXPECT_FLOAT_EQ(ctx.delta_time, 0.f);
    EXPECT_FLOAT_EQ(ctx.total_time, 0.f);
    EXPECT_EQ(ctx.frame_number, 0u);
    EXPECT_FALSE(ctx.has_key("key"));
    EXPECT_FLOAT_EQ(ctx.get_position(1).x, 0.f);
    EXPECT_FLOAT_EQ(ctx.get_health(1), 0.f);
    EXPECT_EQ(ctx.find_entity("test"), INVALID_ENTITY);
    EXPECT_FALSE(ctx.has_item(1, "item"));
    EXPECT_FALSE(ctx.is_key_pressed("w"));
}

// ---------------------------------------------------------------------------
// ScriptRunner — mock script
// ---------------------------------------------------------------------------

class MockHealthScript : public Script {
public:
    ScriptResult tick(const ScriptContext& ctx) override {
        ScriptResult result;
        float health = ctx.get_health(owner());
        if (health < 30.f && ctx.is_alive(owner())) {
            result.heal(owner(), 10.f);
            result.log("Auto-healing low health entity");
        }
        return result;
    }

    std::string name() const override { return "MockHealthScript"; }
};

class MockCombatScript : public Script {
public:
    ScriptResult tick(const ScriptContext& ctx) override {
        ScriptResult result;
        auto nearby = ctx.entities_in_radius(ctx.get_position(owner()), 5.f);
        for (EntityID eid : nearby) {
            if (eid != owner() && ctx.is_alive(eid)) {
                result.damage(eid, 15.f, owner());
            }
        }
        return result;
    }

    std::string name() const override { return "MockCombatScript"; }
};

// ---------------------------------------------------------------------------
// ScriptRunner — attach / detach / tick
// ---------------------------------------------------------------------------

TEST(ScriptRunner, RegisterAndAttach) {
    ScriptRunner runner;
    runner.register_script_class("MockHealthScript", []() {
        return std::make_unique<MockHealthScript>();
    });

    auto result = runner.attach_script("MockHealthScript", 1);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(result.value()->name(), "MockHealthScript");
    EXPECT_EQ(result.value()->owner(), 1u);
    EXPECT_EQ(runner.script_count(), 1u);
}

TEST(ScriptRunner, AttachUnknownClassFails) {
    ScriptRunner runner;
    auto result = runner.attach_script("NonExistent", 1);
    EXPECT_TRUE(result.is_err());
}

TEST(ScriptRunner, DetachScripts) {
    ScriptRunner runner;
    runner.register_script_class("MockHealthScript", []() {
        return std::make_unique<MockHealthScript>();
    });
    runner.attach_script("MockHealthScript", 1);
    runner.attach_script("MockHealthScript", 1);
    EXPECT_EQ(runner.script_count(), 2u);

    runner.detach_scripts(1);
    EXPECT_EQ(runner.script_count(), 0u);
    EXPECT_TRUE(runner.get_scripts(1).empty());
}

TEST(ScriptRunner, DetachNonexistentEntityIsNoOp) {
    ScriptRunner runner;
    runner.detach_scripts(999); // should not crash
    EXPECT_EQ(runner.script_count(), 0u);
}

TEST(ScriptRunner, GetScripts) {
    ScriptRunner runner;
    runner.register_script_class("MockHealthScript", []() {
        return std::make_unique<MockHealthScript>();
    });
    runner.register_script_class("MockCombatScript", []() {
        return std::make_unique<MockCombatScript>();
    });

    runner.attach_script("MockHealthScript", 1);
    runner.attach_script("MockCombatScript", 1);

    auto scripts = runner.get_scripts(1);
    EXPECT_EQ(scripts.size(), 2u);
    EXPECT_TRUE(runner.get_scripts(2).empty());
}

TEST(ScriptRunner, TickEntityCollectsResults) {
    ScriptRunner runner;
    runner.register_script_class("MockHealthScript", []() {
        return std::make_unique<MockHealthScript>();
    });

    runner.attach_script("MockHealthScript", 1);

    ScriptContext ctx;
    ctx.add_entity_health(1, 20.f, 100.f); // low health, alive

    ScriptResult result = runner.tick_entity(1, ctx);
    // Should produce heal + log mutations
    EXPECT_EQ(result.mutation_count(), 2u);
    EXPECT_TRUE(std::holds_alternative<HealEntityMutation>(result.mutations()[0]));
    EXPECT_TRUE(std::holds_alternative<LogMessageMutation>(result.mutations()[1]));
}

TEST(ScriptRunner, TickEntityNoMutationsWhenHealthy) {
    ScriptRunner runner;
    runner.register_script_class("MockHealthScript", []() {
        return std::make_unique<MockHealthScript>();
    });

    runner.attach_script("MockHealthScript", 1);

    ScriptContext ctx;
    ctx.add_entity_health(1, 80.f, 100.f); // healthy

    ScriptResult result = runner.tick_entity(1, ctx);
    EXPECT_TRUE(result.empty());
}

TEST(ScriptRunner, TickAllCollectsFromAllEntities) {
    ScriptRunner runner;
    runner.register_script_class("MockHealthScript", []() {
        return std::make_unique<MockHealthScript>();
    });

    runner.attach_script("MockHealthScript", 1);
    runner.attach_script("MockHealthScript", 2);

    ScriptContext ctx;
    ctx.add_entity_health(1, 20.f, 100.f); // low health
    ctx.add_entity_health(2, 10.f, 100.f); // low health

    ScriptResult result = runner.tick_all(ctx);
    // Both scripts should fire: 2 heals + 2 logs = 4 mutations
    EXPECT_EQ(result.mutation_count(), 4u);
}

TEST(ScriptRunner, TickEntityWithNoScriptsReturnsEmpty) {
    ScriptRunner runner;
    ScriptContext ctx;
    ScriptResult result = runner.tick_entity(999, ctx);
    EXPECT_TRUE(result.empty());
}

// ---------------------------------------------------------------------------
// Full pipeline: combat script targeting nearby entity
// ---------------------------------------------------------------------------

TEST(ScriptRunner, CombatPipelineFullTest) {
    ScriptRunner runner;
    runner.register_script_class("MockCombatScript", []() {
        return std::make_unique<MockCombatScript>();
    });

    runner.attach_script("MockCombatScript", 1);

    ScriptContext ctx;
    ctx.add_entity_position(1, vec3{0.f, 0.f, 0.f});
    ctx.add_entity_position(2, vec3{3.f, 0.f, 0.f}); // in range
    ctx.add_entity_position(3, vec3{100.f, 0.f, 0.f}); // out of range
    ctx.add_entity_health(1, 100.f, 100.f);
    ctx.add_entity_health(2, 50.f, 100.f);
    ctx.add_entity_health(3, 50.f, 100.f);

    ScriptResult result = runner.tick_entity(1, ctx);
    // Should damage entity 2 only (entity 3 out of range, entity 1 is self)
    ASSERT_EQ(result.mutation_count(), 1u);
    const auto& dmg = std::get<DamageEntityMutation>(result.mutations()[0]);
    EXPECT_EQ(dmg.target, 2u);
    EXPECT_FLOAT_EQ(dmg.amount, 15.f);
    EXPECT_EQ(dmg.source, 1u);
}

TEST(ScriptRunner, ClearRemovesEverything) {
    ScriptRunner runner;
    runner.register_script_class("MockHealthScript", []() {
        return std::make_unique<MockHealthScript>();
    });
    runner.attach_script("MockHealthScript", 1);
    EXPECT_EQ(runner.script_count(), 1u);

    runner.clear();
    EXPECT_EQ(runner.script_count(), 0u);
    // Factory also cleared, so attach should fail
    auto result = runner.attach_script("MockHealthScript", 1);
    EXPECT_TRUE(result.is_err());
}

TEST(ScriptRunner, MultipleScriptsOnSameEntity) {
    ScriptRunner runner;
    runner.register_script_class("MockHealthScript", []() {
        return std::make_unique<MockHealthScript>();
    });
    runner.register_script_class("MockCombatScript", []() {
        return std::make_unique<MockCombatScript>();
    });

    runner.attach_script("MockHealthScript", 1);
    runner.attach_script("MockCombatScript", 1);
    EXPECT_EQ(runner.script_count(), 2u);

    ScriptContext ctx;
    ctx.add_entity_position(1, vec3{0.f});
    ctx.add_entity_position(2, vec3{2.f, 0.f, 0.f});
    ctx.add_entity_health(1, 20.f, 100.f); // low health
    ctx.add_entity_health(2, 50.f, 100.f);

    ScriptResult result = runner.tick_entity(1, ctx);
    // MockHealthScript produces heal + log (2), MockCombatScript damages entity 2 (1)
    EXPECT_EQ(result.mutation_count(), 3u);
}
