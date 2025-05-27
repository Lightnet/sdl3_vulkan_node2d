#version 450
layout(location = 0) in vec2 aPos; // Position (x, y)
layout(location = 1) in vec2 aUV;  // Texture coordinates (u, v)

layout(location = 0) out vec2 vUV;  // Pass UV to fragment shader

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0); // Output position in clip space
    vUV = aUV;                         // Pass UV coordinates
}