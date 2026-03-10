#include <gtest/gtest.h>
#include "nadir/behavior_compiler.h"

using odyssey::nadir::compile_behavior_shader;
using odyssey::nadir::generate_shader_preamble;
using odyssey::nadir::prepare_shader_source;

// ---------------------------------------------------------------------------
// generate_shader_preamble
// ---------------------------------------------------------------------------

TEST(ShaderPreamble, ContainsVersion) {
    auto preamble = generate_shader_preamble(256);
    EXPECT_NE(preamble.find("#version 450"), std::string::npos);
}

TEST(ShaderPreamble, ContainsWorkgroupSize) {
    auto preamble = generate_shader_preamble(128);
    EXPECT_NE(preamble.find("128"), std::string::npos);
}

TEST(ShaderPreamble, DefaultWorkgroupSize) {
    // Default (no argument) should embed 256.
    auto preamble = generate_shader_preamble();
    EXPECT_NE(preamble.find("256"), std::string::npos);
}

TEST(ShaderPreamble, ContainsBufferBindings) {
    auto preamble = generate_shader_preamble();
    // At minimum, binding 0 (transforms) and binding 5 (output) must exist.
    EXPECT_NE(preamble.find("binding = 0"), std::string::npos);
    EXPECT_NE(preamble.find("binding = 5"), std::string::npos);
}

TEST(ShaderPreamble, ContainsComputeLayout) {
    auto preamble = generate_shader_preamble(64);
    EXPECT_NE(preamble.find("layout(local_size_x"), std::string::npos);
}

// ---------------------------------------------------------------------------
// prepare_shader_source
// ---------------------------------------------------------------------------

TEST(PrepareSource, CombinesPreambleAndSource) {
    std::string source = "void main() { outputs[0].move_vector = vec4(1.0); }";
    auto full = prepare_shader_source(source);
    // Must contain both the preamble marker and the user source.
    EXPECT_NE(full.find("#version 450"), std::string::npos);
    EXPECT_NE(full.find("void main()"), std::string::npos);
}

TEST(PrepareSource, PreservesUserCode) {
    std::string source = "// CUSTOM MARKER\nvoid main() {}";
    auto full = prepare_shader_source(source, 256);
    EXPECT_NE(full.find("// CUSTOM MARKER"), std::string::npos);
}

TEST(PrepareSource, CustomWorkgroup) {
    auto full = prepare_shader_source("void main() {}", 512);
    EXPECT_NE(full.find("512"), std::string::npos);
}

// ---------------------------------------------------------------------------
// compile_behavior_shader
// ---------------------------------------------------------------------------

TEST(BehaviorCompiler, CompilesValidShader) {
    // Minimal valid Nadir compute shader — the preamble provides all
    // buffer bindings so only the main() body is needed.
    std::string source = R"(
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= total_entities) return;
    outputs[idx].move_vector = vec4(1.0, 0.0, 0.0, 1.0);
}
)";
    auto full_source = prepare_shader_source(source);
    auto result = compile_behavior_shader(full_source, "test_valid", "");
    EXPECT_TRUE(result.success) << "Error: " << result.error_message;
    EXPECT_GT(result.spirv.size(), 0u);
}

TEST(BehaviorCompiler, RejectsInvalidShader) {
    auto result = compile_behavior_shader("invalid {{{{ not glsl",
                                          "test_invalid", "");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST(BehaviorCompiler, RejectsEmptySource) {
    auto result = compile_behavior_shader("", "test_empty", "");
    EXPECT_FALSE(result.success);
}

TEST(BehaviorCompiler, ShaderNamePreserved) {
    auto source = prepare_shader_source(
        "void main() { uint idx = gl_GlobalInvocationID.x; "
        "if (idx >= total_entities) return; "
        "outputs[idx].move_vector = vec4(0.0); }");
    auto result = compile_behavior_shader(source, "my_shader", "");
    EXPECT_EQ(result.shader_name, "my_shader");
}

TEST(BehaviorCompiler, RejectsFragmentShader) {
    // Fragment-only GLSL should fail when compiled as compute.
    std::string frag = R"(
#version 450
layout(location = 0) out vec4 outColor;
void main() { outColor = vec4(1.0); }
)";
    auto result = compile_behavior_shader(frag, "frag_shader", "");
    // Should fail: this is not a compute shader.
    EXPECT_FALSE(result.success);
}
