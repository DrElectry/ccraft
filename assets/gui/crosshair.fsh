#version 330 core

in vec2 out_uv;

uniform sampler2D frame;
uniform sampler2D crosshair;

uniform float width;
uniform float height;

out vec4 fragColor;

void main() {
    vec3 color = texture(frame, out_uv).rgb;

    // convert 32x32 pixels into UV space
    vec2 texSize = vec2(32.0 / width, 32.0 / height);
    vec2 center = vec2(0.5);

    vec2 localUV = (out_uv - center) / texSize + 0.5;

    bool inCrosshair =
        localUV.x >= 0.0 && localUV.x <= 1.0 &&
        localUV.y >= 0.0 && localUV.y <= 1.0;

    if (inCrosshair) {
        vec4 cross = texture(crosshair, localUV);

        if (cross.rgb == vec3(1.0)) {
            color = vec3(1.0) - color;
        }
    }

    fragColor = vec4(color, 1.0);
}