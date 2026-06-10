#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D image;
uniform vec2 direction;
uniform vec2 blur_ratio;

uniform float blurScale;
const int samples = 11;

void main()
{
    vec2 scaled_uv = out_uv * blur_ratio;
    vec2 texel = (1.0 / textureSize(image, 0)) * blurScale;
    
    float weights[] = float[](
        0.0565, 0.0940, 0.1195, 0.1308, 0.1195, 0.0940,
        0.0565, 0.0325, 0.0150, 0.0055, 0.0015
    );
    
    vec3 result = vec3(0.0);
    
    for (int i = 0; i < samples; i++)
    {
        float offset = float(i) * texel.x;
        vec2 sampleOffset = direction * offset;
        
        result += texture(image, scaled_uv + sampleOffset).rgb * weights[i];
        result += texture(image, scaled_uv - sampleOffset).rgb * weights[i];
    }
    
    result -= texture(image, scaled_uv).rgb * weights[0];
    
    fragColor = vec4(result, 1.0);
}