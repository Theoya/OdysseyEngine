#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D screenTexture;

layout(push_constant) uniform CRTParams {
    float time;
    float curvature;       // barrel distortion strength (0.0 = off, 4.0 = heavy)
    float scanline_weight; // scanline darkness (0.0 = off, 1.0 = full)
    float scanline_count;  // number of scanlines (e.g., 480.0)
    float vignette_strength;
    float chromatic_aberration;
    float brightness;
    float flicker_amount;
} crt;

// ---- Barrel distortion (CRT screen curvature) ----
vec2 barrel_distort(vec2 uv, float k) {
    vec2 centered = uv * 2.0 - 1.0;
    float r2 = dot(centered, centered);
    float distort = 1.0 + r2 * k * 0.01;
    centered *= distort;
    return centered * 0.5 + 0.5;
}

// ---- Scanlines ----
float scanline(float y, float count, float weight) {
    float line = sin(y * count * 3.14159265) * 0.5 + 0.5;
    return 1.0 - weight * (1.0 - line * line);
}

// ---- Vignette (darkened edges) ----
float vignette(vec2 uv, float strength) {
    vec2 d = uv - 0.5;
    float r = dot(d, d);
    return 1.0 - r * strength * 4.0;
}

// ---- Phosphor mask (RGB sub-pixel pattern) ----
vec3 phosphor_mask(vec2 fragCoord, float scale) {
    int x = int(mod(fragCoord.x * scale, 3.0));
    vec3 mask = vec3(0.8);
    if (x == 0)      mask = vec3(1.0, 0.7, 0.7);
    else if (x == 1)  mask = vec3(0.7, 1.0, 0.7);
    else              mask = vec3(0.7, 0.7, 1.0);
    return mix(vec3(1.0), mask, 0.35);
}

// ---- Noise (for flicker / static) ----
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    float curv = max(crt.curvature, 0.0);
    float scan_w = clamp(crt.scanline_weight, 0.0, 1.0);
    float scan_n = max(crt.scanline_count, 1.0);
    float vig = max(crt.vignette_strength, 0.0);
    float ca = crt.chromatic_aberration;
    float bright = max(crt.brightness, 0.0);
    float flicker = clamp(crt.flicker_amount, 0.0, 1.0);

    // Apply barrel distortion
    vec2 uv = barrel_distort(fragUV, curv);

    // Kill pixels outside the curved screen
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Chromatic aberration — offset R and B channels
    vec2 ca_offset = (uv - 0.5) * ca * 0.003;
    float r = texture(screenTexture, uv + ca_offset).r;
    float g = texture(screenTexture, uv).g;
    float b = texture(screenTexture, uv - ca_offset).b;
    vec3 color = vec3(r, g, b);

    // Scanlines
    float sl = scanline(uv.y, scan_n, scan_w);
    color *= sl;

    // Phosphor sub-pixel mask
    vec2 screen_res = vec2(textureSize(screenTexture, 0));
    vec3 pmask = phosphor_mask(uv * screen_res, 1.0);
    color *= pmask;

    // Vignette
    float v = vignette(uv, vig);
    color *= v;

    // Brightness boost (compensate for scanline/vignette darkening)
    color *= bright;

    // Flicker (subtle temporal noise)
    float noise = hash(vec2(crt.time * 7.0, uv.y * 100.0));
    float flicker_mod = 1.0 - flicker * 0.03 * (noise - 0.5);
    color *= flicker_mod;

    // Slight bloom / glow on bright areas
    float lum = dot(color, vec3(0.299, 0.587, 0.114));
    vec3 glow = color * smoothstep(0.6, 1.0, lum) * 0.15;
    color += glow;

    // Slight green tint for that old-monitor feel
    color *= vec3(0.95, 1.0, 0.92);

    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
