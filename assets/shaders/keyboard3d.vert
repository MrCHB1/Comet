#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aLeft;
layout (location = 2) in float aRight;
layout (location = 3) in float aPressFactor;
layout (location = 4) in uint aMeta;

uniform mat4 projection;
uniform mat4 view;
uniform float keyboardZOffset;

out vec3 FragPos;
out vec2 TexCoord;
out float pressFactor;
flat out uint meta;

const float keyboardThickness = 0.20f;
const float keyboardHeight = 0.3f;

void main()
{
    meta = aMeta;
    bool isBlack = (aMeta & (1u << 25u)) != 0u;
    pressFactor = aPressFactor;
    
    // 1. Fix White Keys: Create a micro-gap so they don't merge into a single blob
    float rawWidth = aRight - aLeft;
    float gap = isBlack ? 0.0 : 0.001;
    float keyWidth = rawWidth - gap;
    float startX = aLeft + (gap / 2.0);

    // 2. Scale the unit cube
    vec3 localPos = aPos;
    localPos.x *= keyWidth;
    localPos.z *= keyboardHeight;

    // 3. Fix Dimensions: Drastically reduce height to look like standard keys
    localPos.y *= isBlack ? 0.035 : 0.0175; // Much shorter Y height
    localPos.z *= isBlack ? 0.15 : 0.25; // Depth towards the camera

    // 4. Pivot at the back (Z=0)
    const float maxRotation = 0.06; 
    float angle = aPressFactor * maxRotation - maxRotation;
    
    mat4 rotation = mat4(1.0);
    rotation[1][1] = cos(angle);
    rotation[1][2] = sin(angle);
    rotation[2][1] = -sin(angle);
    rotation[2][2] = cos(angle);
   
    localPos.y *= keyboardThickness;
    vec4 rotatedPos = rotation * vec4(localPos, 1.0);

    vec3 worldPos = rotatedPos.xyz;
    worldPos.x += startX;
    
    // Elevate black keys slightly so they sit definitively on top of white keys
    if (isBlack) {
        worldPos.y += 0.001 * keyboardThickness; 
    }

    worldPos.z += keyboardZOffset;

    gl_Position = projection * view * vec4(worldPos, 1.0);
    FragPos = worldPos;
}