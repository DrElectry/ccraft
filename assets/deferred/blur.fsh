#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D image;
uniform vec2 direction;
uniform vec2 blur_ratio;

uniform float blurScale;
const int samples = 17;

void main()
{
    vec2 scaled_uv = out_uv * blur_ratio;
    vec2 texelSize = (1.0 / textureSize(image, 0)) * blurScale;
    
    float weights[] = float[](
        0.0276, 0.0376, 0.0489, 0.0607, 0.0719, 0.0816,
        0.0886, 0.0921, 0.0916, 0.0874, 0.0798, 0.0696,
        0.0578, 0.0458, 0.0347, 0.0251, 0.0173
    );
    
    float wsum = weights[0];
    for (int i = 1; i < samples; i++)
        wsum += 2.0 * weights[i];
    
    vec3 result = vec3(0.0);
    
    for (int i = 0; i < samples; i++)
    {
        vec2 offset = float(i) * texelSize;
        vec2 sampleOffset = direction * offset;
        
        result += texture(image, scaled_uv + sampleOffset).rgb * weights[i];
        result += texture(image, scaled_uv - sampleOffset).rgb * weights[i];
    }
    
    result -= texture(image, scaled_uv).rgb * weights[0];
    
    fragColor = vec4(result / wsum, 1.0);
}