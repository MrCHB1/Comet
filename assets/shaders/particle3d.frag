#version 330 core

in vec2 uv;
in vec4 particleColor;
out vec4 fragColor;

uniform float aspect;

void main()
{
	vec2 p = uv * 2.0 - 1.0;
	p.x *= aspect;
	float dist = length(p);
	float alpha = 1.0 - smoothstep(0.95, 1.0, dist);
	fragColor = vec4(particleColor.rgb, particleColor.a * alpha);
}
