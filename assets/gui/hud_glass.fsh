#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D scene;
uniform sampler2D blurred;
uniform sampler2D buttons;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec4 btn = texture(buttons, out_uv);
    if (btn.a < 0.02) discard;

    vec2 texel = 1.0 / vec2(textureSize(buttons, 0));

    float aC  = btn.a;
    float aL  = texture(buttons, out_uv + vec2(-texel.x, 0.0)).a;
    float aR  = texture(buttons, out_uv + vec2( texel.x, 0.0)).a;
    float aD  = texture(buttons, out_uv + vec2(0.0, -texel.y)).a;
    float aU  = texture(buttons, out_uv + vec2(0.0,  texel.y)).a;
    float aLL = texture(buttons, out_uv + vec2(-texel.x * 2.0, 0.0)).a;
    float aRR = texture(buttons, out_uv + vec2( texel.x * 2.0, 0.0)).a;
    float aDD = texture(buttons, out_uv + vec2(0.0, -texel.y * 2.0)).a;
    float aUU = texture(buttons, out_uv + vec2(0.0,  texel.y * 2.0)).a;
    float aUL = texture(buttons, out_uv + texel * vec2(-2.0,  2.0)).a;
    float aUR = texture(buttons, out_uv + texel * vec2( 2.0,  2.0)).a;
    float aDL = texture(buttons, out_uv + texel * vec2(-2.0, -2.0)).a;
    float aDR = texture(buttons, out_uv + texel * vec2( 2.0, -2.0)).a;

    float smoothA = (aC * 4.0 + (aL + aR + aU + aD) * 2.0 + aLL + aRR + aUU + aDD + aUL + aUR + aDL + aDR) / 24.0;

    vec2 gradA = vec2(
        (aR - aL) * 2.0 + (aRR - aLL) + (aUR - aUL) + (aDR - aDL),
        (aU - aD) * 2.0 + (aUU - aDD) + (aUR - aDR) + (aUL - aDL)
    ) * 0.125;

    float A = clamp(smoothA, 0.001, 0.999);
    float h = sqrt(max(0.0, 2.0 * A - A * A));
    float dh_dA = (1.0 - A) / max(h, 0.1);
    vec2 gradH = gradA * dh_dA * 8.0;

    vec3 N = normalize(vec3(-gradH, 1.0));

    vec3 I = vec3(0.0, 0.0, -1.0);
    float IOR = 1.42;

    vec3 TR = refract(I, N, 1.0 / (IOR + 0.02));
    vec3 TG = refract(I, N, 1.0 / IOR);
    vec3 TB = refract(I, N, 1.0 / (IOR - 0.02));
    if (dot(TR, TR) < 1e-5) TR = I;
    if (dot(TG, TG) < 1e-5) TG = I;
    if (dot(TB, TB) < 1e-5) TB = I;

    float depthScale = 0.16;
    vec2 offR = TR.xy / max(-TR.z, 0.15) * depthScale;
    vec2 offG = TG.xy / max(-TG.z, 0.15) * depthScale;
    vec2 offB = TB.xy / max(-TB.z, 0.15) * depthScale;

    vec2 uvR = clamp(out_uv - offR, 0.0, 1.0);
    vec2 uvG = clamp(out_uv - offG, 0.0, 1.0);
    vec2 uvB = clamp(out_uv - offB, 0.0, 1.0);

    vec3 refracted;
    refracted.r = texture(scene, uvR).r;
    refracted.g = texture(scene, uvG).g;
    refracted.b = texture(scene, uvB).b;

    vec2 btexel = 1.0 / vec2(textureSize(blurred, 0));

    float jitter = hash21(gl_FragCoord.xy * 0.71) - 0.5;
    vec2 frostUV = uvG + vec2(jitter, -jitter) * btexel * 2.0;

    vec2 b1 = btexel * 10.0;
    vec2 b2 = btexel * 24.0;

    vec3 blurBase =
          texture(blurred, frostUV).rgb * 0.24
        + (texture(blurred, frostUV + vec2( b1.x, 0.0)).rgb
         + texture(blurred, frostUV + vec2(-b1.x, 0.0)).rgb
         + texture(blurred, frostUV + vec2(0.0,  b1.y)).rgb
         + texture(blurred, frostUV + vec2(0.0, -b1.y)).rgb) * 0.09
        + (texture(blurred, frostUV + vec2( b1.x,  b1.y)).rgb
         + texture(blurred, frostUV + vec2(-b1.x,  b1.y)).rgb
         + texture(blurred, frostUV + vec2( b1.x, -b1.y)).rgb
         + texture(blurred, frostUV + vec2(-b1.x, -b1.y)).rgb) * 0.06
        + (texture(blurred, frostUV + vec2( b2.x, 0.0)).rgb
         + texture(blurred, frostUV + vec2(-b2.x, 0.0)).rgb
         + texture(blurred, frostUV + vec2(0.0,  b2.y)).rgb
         + texture(blurred, frostUV + vec2(0.0, -b2.y)).rgb) * 0.04;

    float edge = 1.0 - smoothA;
    float edgeMask = smoothstep(0.0, 0.7, edge);

    vec3 col = mix(refracted, blurBase, 0.72 + edgeMask * 0.28);

    float cosI = clamp(N.z, 0.0, 1.0);
    float fresnel = pow(1.0 - cosI, 4.0);

    vec3 envRefl = mix(vec3(0.86, 0.91, 0.98), vec3(1.0), cosI);
    col = mix(col, envRefl, fresnel * 0.40 + 0.05);

    float curvature = length(gradH);
    float specSoft = smoothstep(0.4, 2.0, curvature);
    col += vec3(0.95, 0.98, 1.0) * specSoft * (0.08 + fresnel * 0.22);

    vec3 tint = mix(vec3(0.97, 0.98, 1.0), btn.rgb, 0.10);
    col *= tint;

    col = mix(col, vec3(0.93, 0.96, 1.0), 0.03);

    float thickness = smoothstep(0.6, 1.0, edge);
    col += vec3(0.85, 0.92, 1.0) * thickness * 0.08;

    col *= mix(1.0, 0.98, smoothstep(0.0, 0.3, smoothA));
    col = mix(col, btn.rgb, 0.03);

    col += (hash21(gl_FragCoord.xy) - 0.5) * 0.018;

    float alpha = max(btn.a, 0.68 + edgeMask * 0.20 + fresnel * 0.12);
    alpha = clamp(alpha, 0.0, 1.0);

    fragColor = vec4(col, alpha);
}