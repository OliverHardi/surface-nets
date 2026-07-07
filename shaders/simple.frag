#version 330 core
in vec3 Normal;
in vec3 WorldPos;

out vec4 FragColor;

void main() {
    FragColor = vec4(normalize(Normal) * 0.5 + 0.5, 1.0);
    // FragColor = vec4(1.0, 1.0, 1.0, 0.3);

    // float d = dot(normalize(Normal), normalize(vec3(0.0, 1.0, 0.0)));
    // FragColor = vec4(vec3(d), 1.0);
}