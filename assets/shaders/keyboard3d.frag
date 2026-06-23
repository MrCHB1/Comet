#version 330 core

in vec3 FragPos;
in vec2 TexCoord;
in float pressFactor;
flat in uint meta;

uniform float keyGlowFactor;
uniform vec3 cameraPos;

out vec4 fragColor;

void main()
{
	bool isBlack = (meta & (1u << 25u)) != 0u;
    bool isPressed = (meta & (1u << 24u)) != 0u;

    vec3 color = vec3(
		float((meta & 0xFF0000u) >> 16u), 
		float((meta & 0xFF00u) >> 8u),
		float(meta & 0xFFu)
	) / 255.0f;

    vec3 baseColor = isBlack ? vec3(0.12) : vec3(1.0f);
    vec3 normal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));

    vec3 lightDir = normalize(vec3(0.3, 0.8, 0.5));
    float diffuse = max(dot(normal, lightDir), 0.0);

    if (diffuse < 0.001)
    {
        diffuse = max(dot(-normal, lightDir), 0.0);
    }

    float ambient = 0.25;
    
    float lighting = ambient + (diffuse * 0.75);
    baseColor *= lighting;

    // specular lol
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(
        max(dot(viewDir, reflectDir), 0.0),
        isBlack ? 96.0 : 64.0
    );
    float specStrength = isBlack ? 0.6 : 0.25;
    

    // Highlight actively pressed keys
    if (isPressed) {
        // vec3 pressTint = isBlack ? vec3(0.3, 0.5, 0.8) : vec3(0.6, 0.8, 1.0);

        baseColor = mix(baseColor, color * lighting * keyGlowFactor, pressFactor);
    }
    else
    {
        baseColor += vec3(specular * specStrength);
    }

    fragColor = vec4(baseColor, 1.0);
}