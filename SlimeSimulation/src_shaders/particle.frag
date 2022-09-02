#version 450

layout (binding = 0) uniform sampler2D samplerColorMap;

layout (location = 0) in vec4 inColor;

layout (location = 0) out vec4 outFragColor;

void main () 
{
	//outFragColor.rgb = texture(samplerColorMap, gl_PointCoord).rgb * inColor.rgb;
	outFragColor.rgb = inColor.rgb;
}