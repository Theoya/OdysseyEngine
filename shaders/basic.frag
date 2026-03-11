#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional lighting
    vec3 light_dir = normalize(vec3(0.4, 0.8, 0.3));
    vec3 normal = normalize(fragNormal);

    // Ambient + diffuse
    float ambient = 0.15;
    float diffuse = max(dot(normal, light_dir), 0.0);

    // Secondary fill light from below-left for softer shadows
    vec3 fill_dir = normalize(vec3(-0.3, -0.2, 0.6));
    float fill = max(dot(normal, fill_dir), 0.0) * 0.15;

    float lighting = ambient + diffuse * 0.75 + fill;
    lighting = clamp(lighting, 0.0, 1.0);

    vec3 lit_color = fragColor.rgb * lighting;
    outColor = vec4(lit_color, fragColor.a);
}
