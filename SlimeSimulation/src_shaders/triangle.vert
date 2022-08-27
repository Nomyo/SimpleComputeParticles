#version 450

#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition.xyz, 1.0);
    fragColor = vec3(1.0f * abs(inPosition.y), 1.0f * abs(inPosition.x), 1.0f * abs(inPosition.y));
}
