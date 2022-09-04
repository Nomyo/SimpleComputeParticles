#version 450

#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inVel;

layout(location = 0) out vec4 fragColor;

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
};

void main() {
    gl_PointSize = .1;
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);

	float velocityFactor = length(inVel * 0.2);
    fragColor = vec4(velocityFactor *1.5* 0.254, velocityFactor * 0.988, velocityFactor * 0.788, 1.0);
    //fragColor = vec4(65./255.,252./255.,201./255.,1.0);//vec4(abs(sin(inPosition.x)), 0.4f,inPosition.y, 1.0f);
}
