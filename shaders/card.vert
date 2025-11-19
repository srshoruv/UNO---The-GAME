#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform vec2 offset;
uniform vec2 scale;

void main() {
    gl_Position = vec4(aPos * scale + offset, 0.0, 1.0);
    TexCoord = aTexCoord;
}