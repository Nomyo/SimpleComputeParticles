#version 450

#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inVel;

layout (binding = 1) uniform UBO
{
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
} ubo;

layout(location = 0) out vec4 fragColor;

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    gl_PointSize = .5;
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(inPosition.xyz, 1.0);

    float velocityFactor = length(abs(inVel) * 0.02);
    fragColor = vec4(1.0 * velocityFactor, 1.0 - (0.5* velocityFactor), 1.0 - (velocityFactor), 1.0);
}
