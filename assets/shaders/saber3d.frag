#version 330 core
out vec4 FragColor;

uniform vec3 saberColor;

void main() {
    FragColor = vec4(saberColor, 1.0);
}
