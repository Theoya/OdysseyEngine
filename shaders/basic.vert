// OdysseyEngine — forward vertex shader (basic.vert)
// Phase 6: passes UV coordinates to fragment shader for bindless texture sampling.

#version 450

layout(push_constant) uniform PushConstants {
    mat4     mvp;
    vec4     color;
    uint     material_index;
    float    _pad0;
    float    _pad1;
    float    _pad2;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;       // texture coordinates (may be zero for untextured)

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec2 fragUV;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragNormal  = inNormal;
    fragColor   = pc.color;
    fragUV      = inUV;
}
