#version 450
layout(location = 0) in vec2 vUV; // UV from vertex shader

layout(location = 0) out vec4 fragColor; // Output color

layout(set = 0, binding = 0) uniform sampler2D texSampler; // Texture sampler

void main() {
    fragColor = texture(texSampler, vUV); // Sample texture
}