#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D screenTexture;

layout(push_constant) uniform EvaParams {
    float time;
    float alert_level;     // 0.0 = normal, 1.0 = full NERV alert
    float sync_ratio;      // 0.0-1.0 sync meter
    float health_pct;      // 0.0-1.0 entity health
    float scan_speed;      // scrolling data speed
    float border_width;    // HUD border thickness
    float opacity;         // HUD overlay opacity
    float _pad;
} eva;

// ---- Utility ----
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float hash1(float n) {
    return fract(sin(n) * 43758.5453123);
}

// ---- Hex character (fake 4x5 grid) ----
float hex_char(vec2 uv, float seed) {
    ivec2 cell = ivec2(floor(uv * vec2(4.0, 5.0)));
    if (cell.x < 0 || cell.x >= 4 || cell.y < 0 || cell.y >= 5) return 0.0;
    float bit = hash(vec2(float(cell.x + cell.y * 4), seed));
    return step(0.45, bit);
}

// ---- Scrolling hex data columns ----
float hex_rain(vec2 uv, float t, float columns, float char_size) {
    vec2 grid = uv * vec2(columns, columns * 2.0);
    grid.y += t * 3.0;
    vec2 cell_id = floor(grid);
    vec2 cell_uv = fract(grid);

    float seed = hash(cell_id) * 100.0 + floor(t * 2.0 + hash(cell_id.xx) * 5.0);
    float ch = hex_char(cell_uv * 1.2 - 0.1, seed);

    // Fade based on column position
    float col_brightness = hash(vec2(cell_id.x, 0.0)) * 0.5 + 0.5;
    return ch * col_brightness;
}

// ---- Angular border lines (NERV-style angled panels) ----
float angular_border(vec2 uv, float width) {
    float result = 0.0;

    // Top-left diagonal cut
    float diag_tl = uv.x + uv.y;
    result += smoothstep(width + 0.002, width, abs(diag_tl - 0.15));

    // Bottom-right diagonal cut
    float diag_br = (1.0 - uv.x) + (1.0 - uv.y);
    result += smoothstep(width + 0.002, width, abs(diag_br - 0.15));

    // Horizontal lines
    result += smoothstep(width, width - 0.001, abs(uv.y - 0.08));
    result += smoothstep(width, width - 0.001, abs(uv.y - 0.92));

    // Vertical frame lines
    result += smoothstep(width, width - 0.001, abs(uv.x - 0.03));
    result += smoothstep(width, width - 0.001, abs(uv.x - 0.97));

    // Mid-panel divider (left side)
    if (uv.x < 0.2) {
        result += smoothstep(width, width - 0.001, abs(uv.x - 0.18));
        // Horizontal ticks
        float tick_y = fract(uv.y * 20.0);
        if (uv.x < 0.06 && uv.y > 0.1 && uv.y < 0.9) {
            result += smoothstep(0.02, 0.01, abs(tick_y - 0.5)) * 0.5;
        }
    }

    // Right panel divider
    if (uv.x > 0.8) {
        result += smoothstep(width, width - 0.001, abs(uv.x - 0.82));
    }

    return clamp(result, 0.0, 1.0);
}

// ---- Warning chevrons (diagonal hazard stripes) ----
float warning_stripes(vec2 uv, float t) {
    float stripe = sin((uv.x - uv.y) * 40.0 + t * 2.0) * 0.5 + 0.5;
    return step(0.5, stripe);
}

// ---- Sync ratio bar ----
float sync_bar(vec2 uv, float ratio, float y_pos, float height) {
    if (abs(uv.y - y_pos) > height * 0.5) return 0.0;
    if (uv.x < 0.04 || uv.x > 0.17) return 0.0;

    float bar_uv = (uv.x - 0.04) / 0.13;

    // Bar fill
    float fill = step(bar_uv, ratio);

    // Segment lines
    float segments = step(0.48, fract(bar_uv * 10.0));
    fill *= (1.0 - segments * 0.3);

    return fill;
}

// ---- Target reticle ----
float target_reticle(vec2 uv, float t) {
    vec2 center = vec2(0.5, 0.5);
    float dist = length((uv - center) * vec2(1.0, 0.5625)); // aspect correct

    float ring1 = smoothstep(0.003, 0.001, abs(dist - 0.08));
    float ring2 = smoothstep(0.002, 0.001, abs(dist - 0.12));

    // Rotating dashes
    float angle = atan(uv.y - center.y, uv.x - center.x);
    float dash = step(0.5, fract((angle + t * 0.5) * 4.0 / 6.28318));
    ring2 *= dash;

    // Crosshair lines (thin)
    float cross_h = smoothstep(0.0015, 0.0005, abs(uv.y - 0.5)) *
                    step(0.03, abs(uv.x - 0.5)) * step(abs(uv.x - 0.5), 0.15);
    float cross_v = smoothstep(0.001, 0.0005, abs(uv.x - 0.5)) *
                    step(0.02, abs(uv.y - 0.5)) * step(abs(uv.y - 0.5), 0.08);

    return ring1 + ring2 + cross_h + cross_v;
}

// ---- Scrolling text line ----
float text_line(vec2 uv, float y_pos, float t, float seed) {
    if (abs(uv.y - y_pos) > 0.006) return 0.0;
    float x_scroll = fract(uv.x + t * 0.1 * (seed + 1.0));
    return hex_char(vec2(x_scroll * 30.0, (uv.y - y_pos + 0.006) / 0.012 * 5.0), seed + floor(t));
}

void main() {
    float t = eva.time;
    float alert = clamp(eva.alert_level, 0.0, 1.0);
    float sync = clamp(eva.sync_ratio, 0.0, 1.0);
    float hp = clamp(eva.health_pct, 0.0, 1.0);
    float bw = max(eva.border_width, 0.001);
    float alpha = clamp(eva.opacity, 0.0, 1.0);

    // Sample the scene
    vec3 scene = texture(screenTexture, fragUV).rgb;

    // ---- HUD color palette (Evangelion) ----
    vec3 col_orange  = vec3(1.0, 0.45, 0.05);
    vec3 col_red     = vec3(0.9, 0.1, 0.05);
    vec3 col_green   = vec3(0.1, 0.9, 0.2);
    vec3 col_cyan    = vec3(0.1, 0.7, 0.9);
    vec3 col_yellow  = vec3(1.0, 0.85, 0.1);

    // Alert interpolation: normal = orange, alert = red
    vec3 hud_primary = mix(col_orange, col_red, alert);
    vec3 hud_accent = mix(col_green, col_yellow, alert);

    // ---- Composite HUD elements ----
    vec3 hud = vec3(0.0);
    float hud_mask = 0.0;

    // 1. Angular borders
    float borders = angular_border(fragUV, bw);
    hud += hud_primary * borders;
    hud_mask += borders;

    // 2. Hex data rain (left panel)
    if (fragUV.x < 0.17 && fragUV.y > 0.1 && fragUV.y < 0.9) {
        vec2 panel_uv = (fragUV - vec2(0.03, 0.1)) / vec2(0.14, 0.8);
        float hex = hex_rain(panel_uv, t * eva.scan_speed, 8.0, 1.0);
        hud += hud_primary * hex * 0.4;
        hud_mask += hex * 0.3;
    }

    // 3. Hex data rain (right panel)
    if (fragUV.x > 0.83 && fragUV.y > 0.1 && fragUV.y < 0.9) {
        vec2 panel_uv = (fragUV - vec2(0.83, 0.1)) / vec2(0.14, 0.8);
        float hex = hex_rain(panel_uv, t * eva.scan_speed * 1.3 + 50.0, 6.0, 1.0);
        hud += col_cyan * hex * 0.3;
        hud_mask += hex * 0.25;
    }

    // 4. Sync ratio bar (left)
    float sb = sync_bar(fragUV, sync, 0.85, 0.025);
    vec3 bar_color = mix(col_red, col_green, sync);
    hud += bar_color * sb;
    hud_mask += sb;

    // 5. Health bar (left, below sync)
    float hb = sync_bar(fragUV, hp, 0.81, 0.02);
    vec3 hp_color = mix(col_red, hud_accent, hp);
    hud += hp_color * hb;
    hud_mask += hb;

    // 6. Target reticle
    float reticle = target_reticle(fragUV, t);
    hud += hud_primary * reticle;
    hud_mask += reticle;

    // 7. Scrolling text lines (bottom)
    for (int i = 0; i < 3; i++) {
        float y = 0.94 + float(i) * 0.015;
        float tl = text_line(fragUV, y, t, float(i) * 17.3);
        hud += hud_primary * tl * 0.6;
        hud_mask += tl * 0.4;
    }

    // 8. Warning stripes on alert (top and bottom edges)
    if (alert > 0.3) {
        float stripe_zone = step(fragUV.y, 0.03) + step(0.97, fragUV.y);
        float stripes = warning_stripes(fragUV, t) * stripe_zone;
        float alert_flash = 0.5 + 0.5 * sin(t * 6.0);
        hud += col_red * stripes * alert * alert_flash;
        hud_mask += stripes * alert * 0.6;
    }

    // 9. Corner brackets (targeting frame)
    float bracket_len = 0.06;
    float bracket_thick = 0.002;
    vec2 tl = vec2(0.3, 0.3);
    vec2 br = vec2(0.7, 0.7);
    float brackets = 0.0;
    // Top-left
    brackets += smoothstep(bracket_thick, 0.0, abs(fragUV.y - tl.y)) * step(tl.x, fragUV.x) * step(fragUV.x, tl.x + bracket_len);
    brackets += smoothstep(bracket_thick, 0.0, abs(fragUV.x - tl.x)) * step(tl.y, fragUV.y) * step(fragUV.y, tl.y + bracket_len);
    // Top-right
    brackets += smoothstep(bracket_thick, 0.0, abs(fragUV.y - tl.y)) * step(br.x - bracket_len, fragUV.x) * step(fragUV.x, br.x);
    brackets += smoothstep(bracket_thick, 0.0, abs(fragUV.x - br.x)) * step(tl.y, fragUV.y) * step(fragUV.y, tl.y + bracket_len);
    // Bottom-left
    brackets += smoothstep(bracket_thick, 0.0, abs(fragUV.y - br.y)) * step(tl.x, fragUV.x) * step(fragUV.x, tl.x + bracket_len);
    brackets += smoothstep(bracket_thick, 0.0, abs(fragUV.x - tl.x)) * step(br.y - bracket_len, fragUV.y) * step(fragUV.y, br.y);
    // Bottom-right
    brackets += smoothstep(bracket_thick, 0.0, abs(fragUV.y - br.y)) * step(br.x - bracket_len, fragUV.x) * step(fragUV.x, br.x);
    brackets += smoothstep(bracket_thick, 0.0, abs(fragUV.x - br.x)) * step(br.y - bracket_len, fragUV.y) * step(fragUV.y, br.y);
    hud += hud_primary * clamp(brackets, 0.0, 1.0) * 0.8;
    hud_mask += brackets * 0.5;

    // ---- Final composite ----
    // Slight scanline over entire screen for that monitor feel
    float scanline = 0.95 + 0.05 * sin(fragUV.y * 800.0);
    scene *= scanline;

    // Composite HUD over scene
    vec3 final_color = mix(scene, scene + hud, alpha * clamp(hud_mask, 0.0, 1.0));

    outColor = vec4(clamp(final_color, 0.0, 1.0), 1.0);
}
