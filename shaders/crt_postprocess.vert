#version 450

layout(location = 0) out vec2 fragUV;

// Full-screen triangle (no vertex buffer needed)
void main() {
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
    fragUV.y = 1.0 - fragUV.y; // flip Y for Vulkan
}
