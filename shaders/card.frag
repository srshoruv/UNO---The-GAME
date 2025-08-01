#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform vec3 color;
uniform float highlight;
uniform sampler2D ourTexture;
uniform int hasTexture;
uniform int isWild;

void main() {
    vec4 texColor = texture(ourTexture, TexCoord);
    vec4 baseColor = vec4(color, 1.0);

    vec4 finalColor = texColor;

    if (hasTexture > 0) {
        if (texColor.r > 0.9 && texColor.g > 0.9 && texColor.b > 0.9) {
            finalColor = baseColor;
        }
    }

    if (highlight > 0.5)
        FragColor = vec4(finalColor.rgb * 0.7 + vec3(0.3,0.3,0.3), 1.0);
    else
        FragColor = finalColor;
}