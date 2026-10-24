
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

    vec2 texel = 1.0 / vec2(textureSize(buttons, 0));

    vec2 shadowOffset = vec2(0.0, 5.0) * texel;
    vec2 shadowUV = out_uv + shadowOffset;

    float shadowA = 0.0;
    float shadowWeight = 0.0;

    for (int x = -4; x <= 4; x++) {
        for (int y = -4; y <= 4; y++) {
            vec2 offset = vec2(float(x), float(y)) * texel * 2.5;
            float weight = exp(-dot(vec2(x, y), vec2(x, y)) / 12.0);

            shadowA += texture(buttons, shadowUV + offset).a * weight;
            shadowWeight += weight;
        }
    }

    shadowA /= shadowWeight;

    float rawAlpha = clamp(btn.a, 0.0, 1.0);
    float glassOpacity = pow(rawAlpha, 0.45);
    float glassStrength = smoothstep(0.0, 0.45, rawAlpha);

    float shadow = shadowA * (1.0 - rawAlpha) * 0.22;

    if (rawAlpha < 0.02) {
        if (shadow < 0.002)
            discard;

        fragColor = vec4(vec3(0.025, 0.035, 0.055), shadow);
        return;
    }

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

    float smoothA = (
        aC * 4.0 +
        (aL + aR + aU + aD) * 2.0 +
        aLL + aRR + aUU + aDD +
        aUL + aUR + aDL + aDR
    ) / 24.0;

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

    float depthScale = 0.16 * glassStrength;

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
        + (
            texture(blurred, frostUV + vec2( b1.x, 0.0)).rgb
          + texture(blurred, frostUV + vec2(-b1.x, 0.0)).rgb
          + texture(blurred, frostUV + vec2(0.0,  b1.y)).rgb
          + texture(blurred, frostUV + vec2(0.0, -b1.y)).rgb
        ) * 0.09
        + (
            texture(blurred, frostUV + vec2( b1.x,  b1.y)).rgb
          + texture(blurred, frostUV + vec2(-b1.x,  b1.y)).rgb
          + texture(blurred, frostUV + vec2( b1.x, -b1.y)).rgb
          + texture(blurred, frostUV + vec2(-b1.x, -b1.y)).rgb
        ) * 0.06
        + (
            texture(blurred, frostUV + vec2( b2.x, 0.0)).rgb
          + texture(blurred, frostUV + vec2(-b2.x, 0.0)).rgb
          + texture(blurred, frostUV + vec2(0.0,  b2.y)).rgb
          + texture(blurred, frostUV + vec2(0.0, -b2.y)).rgb
        ) * 0.04;

    blurBase /= 0.96;

    float edge = 1.0 - smoothA;
    float edgeMask = smoothstep(0.0, 0.7, edge);

    float frostStrength = smoothstep(0.0, 0.85, rawAlpha);

    vec3 frostedGlass = mix(
        refracted,
        blurBase,
        0.65 + 0.35 * frostStrength
    );

    vec3 col = mix(
        refracted,
        frostedGlass,
        frostStrength
    );

    float cosI = clamp(N.z, 0.0, 1.0);
    float fresnel = pow(1.0 - cosI, 4.0);

    vec3 envRefl = mix(
        vec3(0.42, 0.47, 0.54),
        vec3(0.72, 0.76, 0.82),
        cosI
    );

    col = mix(
        col,
        envRefl,
        (fresnel * 0.22 + 0.025) * glassStrength
    );

    float curvature = length(gradH);
    float specSoft = smoothstep(0.4, 2.0, curvature);

    col += vec3(0.65, 0.70, 0.76)
         * specSoft
         * (0.025 + fresnel * 0.08)
         * glassStrength;

    vec3 tint = mix(
        vec3(0.88, 0.90, 0.93),
        btn.rgb,
        0.08
    );

    col *= tint;

    float thickness = smoothstep(0.6, 1.0, edge);

    col += vec3(0.55, 0.62, 0.70)
         * thickness
         * 0.025
         * glassStrength;

    col *= mix(
        1.0,
        0.98,
        smoothstep(0.0, 0.3, smoothA) * glassStrength
    );

    col = mix(col, btn.rgb, 0.03 * glassStrength);

    col += (hash21(gl_FragCoord.xy) - 0.5)
         * 0.012
         * glassStrength;

    col *= 0.94;

    float alpha = glassOpacity;

    fragColor = vec4(clamp(col, 0.0, 1.0), alpha);
}
