#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform UBO 
{
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
} ubo;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main()
{
    outColor = vec4(inColor, 1.0f);
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(inPos, 1.0f);
}