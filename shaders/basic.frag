// OdysseyEngine — forward fragment shader (basic.frag)
// Phase 6: bindless texture sampling via set=0 combined-image-sampler array.
//
// Lighting model: Lambertian diffuse + Blinn-Phong specular.
// This is NOT a PBR G-buffer. No metallic, roughness, or normal maps are read.
// (Mandate 4 + vibe-story-guardian condition: no PBR drift.)
//
// Descriptor layout (vibe comment, binding contract):
//   set=0, binding=0 : texture2D bindless_textures[16384]  (this file)
//   set=1            : per-frame UBOs (camera, lighting, CRT)
//   push constants   : MVP (mat4), color (vec4), material_index (uint)
//
// nonuniformEXT derivation (SPIR-V spec 14.1.1 / GL_EXT_nonuniform_qualifier):
//   When a shader invocation indexes an array with a value that may differ
//   across a GPU wave (divergent index), the driver must treat each lane
//   independently for descriptor table lookups.  Without the NonUniform
//   decoration in SPIR-V, the hardware is permitted to assume all lanes
//   in the wave share the same index — leading to undefined behaviour when
//   they don't.  The NVIDIA wave-coherence rule (SIMT lane grouping) makes
//   this a hard correctness requirement, not just a performance hint.
//   Mitigation: annotate every dynamic-index sample site with nonuniformEXT().

#version 450
#extension GL_EXT_nonuniform_qualifier : require

// set=0, binding=0: bindless texture array (MAX_BINDLESS_TEXTURES = 16384 slots).
// Slot 0 is the 1×1 magenta sentinel (RGBA 1,0,1,1).
layout(set = 0, binding = 0) uniform sampler2D bindless_textures[];

// Push constants match BasicPushConstants in renderer.h + BasicPushConstantsExtended.
// material_index: lower 16 bits = bindless slot.  0 = sentinel (use vertex color).
layout(push_constant) uniform PushConstants {
    mat4     mvp;
    vec4     color;
    uint     material_index;
    float    _pad0;
    float    _pad1;
    float    _pad2;
} pc;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    // ---------------------------------------------------------------------------
    // Lighting: Lambertian diffuse + Blinn-Phong specular.
    //
    // Derivation (M3 first-principles):
    //   Lambertian reflectance: L_d = k_d * max(dot(N, L), 0)
    //     where k_d = diffuse coefficient, N = surface normal, L = light direction.
    //
    //   Blinn-Phong specular: L_s = k_s * max(dot(N, H), 0)^shininess
    //     where H = normalize(L + V) is the half-vector,
    //           V = view direction (approximated as (0,0,1) in view space for now),
    //           k_s = specular coefficient.
    //
    //   Total irradiance: L = ambient + L_d + L_s
    //
    //   Constants chosen for the showcase scene lighting feel:
    //     ambient = 0.15 (ensures no fully-black shadow regions)
    //     k_d     = 0.75 (dominant diffuse)
    //     k_s     = 0.20, shininess = 32
    //     fill light factor = 0.15 (secondary fill from below-left)
    // ---------------------------------------------------------------------------

    vec3 light_dir = normalize(vec3(0.4, 0.8, 0.3));
    vec3 normal    = normalize(fragNormal);

    float ambient = 0.15;
    float diffuse = max(dot(normal, light_dir), 0.0);

    // Blinn-Phong half-vector (view approximated as (0,0,1) in world-ish space).
    vec3 view_dir  = vec3(0.0, 0.0, 1.0);
    vec3 half_vec  = normalize(light_dir + view_dir);
    float specular = pow(max(dot(normal, half_vec), 0.0), 32.0) * 0.20;

    // Secondary fill light from below-left (softens harsh shadows).
    vec3  fill_dir = normalize(vec3(-0.3, -0.2, 0.6));
    float fill     = max(dot(normal, fill_dir), 0.0) * 0.15;

    float lighting = clamp(ambient + diffuse * 0.75 + specular + fill, 0.0, 1.0);

    // ---------------------------------------------------------------------------
    // Texture sampling via nonuniformEXT (bindless).
    //
    // material_index == 0 means no texture — use vertex color (fragColor).
    // material_index > 0 means sample from the bindless array at that slot.
    //
    // nonuniformEXT MANDATORY: material_index varies per-draw-call (and in
    // future multi-draw-indirect across different materials in the same frame),
    // so it is dynamically non-uniform across waves.  Without NonUniform
    // decoration the GPU may read the wrong descriptor.  See file header.
    // ---------------------------------------------------------------------------
    vec4 base_color;
    if (pc.material_index == 0u) {
        // No texture — use push-constant color (or interpolated vertex color).
        base_color = fragColor;
    } else {
        // Sample from the bindless array with nonuniformEXT qualifier.
        // nonuniformEXT wraps the index expression; SPIR-V emits NonUniform
        // decoration on the access chain (SPIR-V spec 14.1.1).
        uint tex_slot = pc.material_index & 0xFFFFu; // lower 16 bits = slot
        base_color = texture(bindless_textures[nonuniformEXT(tex_slot)], fragUV);
    }

    vec3 lit_color = base_color.rgb * lighting;
    outColor = vec4(lit_color, base_color.a);
}
