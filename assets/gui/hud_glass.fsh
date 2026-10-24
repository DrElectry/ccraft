#version 330 core

in vec2 out_uv;
out vec4 fragColor;

uniform sampler2D scene;
uniform sampler2D blurred;
uniform sampler2D buttons;

const float TAU = 6.28318531;
const float GOLDEN_ANGLE = 2.39996323;

const float SLOPE_MAX = 1.5;
const float REFRACT_CENTER = 0.015;
const float REFRACT_EDGE = 0.12;
const float DISPERSION = 0.035;
const float BLUR_RADIUS = 32.0;

const float RIM_NEAR = 2.0;
const float RIM_FAR = 5.0;

const float INTERIOR_FROST = 0.42;
const float RIM_FROST = 0.48;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

vec3 frostScatter(
    sampler2D tex,
    vec2 uv,
    vec2 texel,
    float radius,
    float rot
) {
    const int TAPS = 24;

    float sigma = radius * 0.45;
    float invSigma2 = 1.0 / max(2.0 * sigma * sigma, 1e-5);

    vec3 sum = texture(tex, uv).rgb;
    float wsum = 1.0;

    for (int i = 0; i < TAPS; i++) {
        float fi = float(i) + 0.5;
        float r = sqrt(fi / float(TAPS)) * radius;
        float a = fi * GOLDEN_ANGLE + rot;

        vec2 off = vec2(cos(a), sin(a)) * r * texel;
        float w = exp(-r * r * invSigma2);

        sum += texture(
            tex,
            clamp(uv + off, vec2(0.0), vec2(1.0))
        ).rgb * w;

        wsum += w;
    }

    return sum / wsum;
}

float borderBand(vec2 uv, vec2 texel) {
    const int DIRS = 12;

    float nearA = 1.0;
    float farA = 1.0;

    for (int i = 0; i < DIRS; i++) {
        float ang = float(i) * TAU / float(DIRS);
        vec2 dir = vec2(cos(ang), sin(ang));

        nearA = min(
            nearA,
            texture(
                buttons,
                clamp(uv + dir * RIM_NEAR * texel, vec2(0.0), vec2(1.0))
            ).a
        );

        farA = min(
            farA,
            texture(
                buttons,
                clamp(uv + dir * RIM_FAR * texel, vec2(0.0), vec2(1.0))
            ).a
        );
    }

    return clamp(
        max(1.0 - nearA, (1.0 - farA) * 0.55),
        0.0,
        1.0
    );
}

float shadowBlur(vec2 uv, vec2 texel) {
    float s = 0.0;

    s += texture(buttons, uv).a * 0.20;

    s += texture(buttons, uv + vec2( 3.0,  0.0) * texel).a * 0.12;
    s += texture(buttons, uv + vec2(-3.0,  0.0) * texel).a * 0.12;
    s += texture(buttons, uv + vec2( 0.0,  3.0) * texel).a * 0.12;
    s += texture(buttons, uv + vec2( 0.0, -3.0) * texel).a * 0.12;

    s += texture(buttons, uv + vec2( 3.0,  3.0) * texel).a * 0.08;
    s += texture(buttons, uv + vec2(-3.0,  3.0) * texel).a * 0.08;
    s += texture(buttons, uv + vec2( 3.0, -3.0) * texel).a * 0.08;
    s += texture(buttons, uv + vec2(-3.0, -3.0) * texel).a * 0.08;

    return s;
}

void main() {
    vec4 btn = texture(buttons, out_uv);
    vec2 texel = 1.0 / vec2(textureSize(buttons, 0));

    vec2 shadowUV = out_uv + vec2(0.0, 5.0) * texel;
    float shadowA = shadowBlur(shadowUV, texel);

    float rawAlpha = clamp(btn.a, 0.0, 1.0);
    float glassOpacity = rawAlpha;

    float shadowCore = pow(shadowA, 1.35);
    float shadow = shadowCore * (1.0 - rawAlpha) * 0.38;

    if (rawAlpha < 0.02) {
        if (shadow < 0.002)
            discard;

        fragColor = vec4(
            vec3(0.025, 0.035, 0.055),
            shadow
        );
        return;
    }

    float aC = rawAlpha;

    float aL = texture(buttons, out_uv + vec2(-texel.x, 0.0)).a;
    float aR = texture(buttons, out_uv + vec2( texel.x, 0.0)).a;
    float aD = texture(buttons, out_uv + vec2(0.0, -texel.y)).a;
    float aU = texture(buttons, out_uv + vec2(0.0,  texel.y)).a;

    float aLL = texture(buttons, out_uv + vec2(-texel.x * 2.0, 0.0)).a;
    float aRR = texture(buttons, out_uv + vec2( texel.x * 2.0, 0.0)).a;
    float aDD = texture(buttons, out_uv + vec2(0.0, -texel.y * 2.0)).a;
    float aUU = texture(buttons, out_uv + vec2(0.0,  texel.y * 2.0)).a;

    float aUL = texture(buttons, out_uv + texel * vec2(-2.0,  2.0)).a;
    float aUR = texture(buttons, out_uv + texel * vec2( 2.0,  2.0)).a;
    float aDL = texture(buttons, out_uv + texel * vec2(-2.0, -2.0)).a;
    float aDR = texture(buttons, out_uv + texel * vec2( 2.0, -2.0)).a;

    float smoothA = (
        aC * 4.0
        + (aL + aR + aU + aD) * 2.0
        + aLL + aRR + aUU + aDD
        + aUL + aUR + aDL + aDR
    ) / 24.0;

    vec2 gradA = vec2(
        (aR - aL) * 2.0
        + (aRR - aLL)
        + (aUR - aUL)
        + (aDR - aDL),

        (aU - aD) * 2.0
        + (aUU - aDD)
        + (aUR - aDR)
        + (aUL - aDL)
    ) * 0.125;

    float rimMask = borderBand(out_uv, texel);
    float rimGlass = rimMask;

    float A = clamp(smoothA, 0.001, 0.999);
    float h = sqrt(max(0.0, 2.0 * A - A * A));
    float dh_dA = (1.0 - A) / max(h, 0.1);

    vec2 gradTight = vec2(
        aR - aL,
        aU - aD
    ) * 0.5;

    vec2 gradH = mix(
        gradA,
        gradTight,
        rimGlass * 0.75
    ) * dh_dA * 6.0;

    float slope = length(gradH);

    gradH *= min(slope, SLOPE_MAX) / max(slope, 1e-5);

    vec3 N = normalize(vec3(-gradH, 1.0));
    vec3 I = vec3(0.0, 0.0, -1.0);

    float IOR = 1.42;
    float disp = DISPERSION * (0.5 + rimGlass);

    vec3 TR = refract(I, N, 1.0 / (IOR + disp));
    vec3 TG = refract(I, N, 1.0 / IOR);
    vec3 TB = refract(I, N, 1.0 / (IOR - disp));

    if (dot(TR, TR) < 1e-5) TR = I;
    if (dot(TG, TG) < 1e-5) TG = I;
    if (dot(TB, TB) < 1e-5) TB = I;

    float depthScale = mix(
        REFRACT_CENTER,
        REFRACT_EDGE,
        rimGlass
    );

    const float Z_FLOOR = 0.35;

    vec2 offR = TR.xy / max(-TR.z, Z_FLOOR) * depthScale;
    vec2 offG = TG.xy / max(-TG.z, Z_FLOOR) * depthScale;
    vec2 offB = TB.xy / max(-TB.z, Z_FLOOR) * depthScale;

    vec2 lens = gradH * 0.012;

    offR += lens * 1.08;
    offG += lens;
    offB += lens * 0.92;

    vec2 uvR = clamp(out_uv - offR, vec2(0.0), vec2(1.0));
    vec2 uvG = clamp(out_uv - offG, vec2(0.0), vec2(1.0));
    vec2 uvB = clamp(out_uv - offB, vec2(0.0), vec2(1.0));

    vec3 refracted;

    refracted.r = texture(scene, uvR).r;
    refracted.g = texture(scene, uvG).g;
    refracted.b = texture(scene, uvB).b;

    vec2 btexel = 1.0 / vec2(textureSize(blurred, 0));

    float rot = hash21(gl_FragCoord.xy * 0.71) * TAU;

    vec3 blurredRefracted = frostScatter(
        blurred,
        uvG,
        btexel,
        BLUR_RADIUS,
        rot
    );

    float frostAmount = mix(
        INTERIOR_FROST,
        RIM_FROST,
        rimMask
    );

    frostAmount *= smoothstep(0.0, 0.35, rawAlpha);

    frostAmount += (
        hash21(gl_FragCoord.xy + 7.13) - 0.5
    ) * 0.04;

    frostAmount = clamp(frostAmount, 0.0, 1.0);

    vec3 glassBody = mix(
        refracted,
        blurredRefracted,
        frostAmount
    );

    float opaqueStrength = clamp(
        (1.0 - rawAlpha) * 0.0,
        0.0,
        1.0
    );

    glassBody = mix(
        glassBody,
        btn.rgb,
        opaqueStrength
    );

    float curvature = length(gradH);
    float specSoft = smoothstep(0.4, 1.5, curvature);

    glassBody += vec3(1.0)
        * specSoft
        * 0.025;

    float topEdge =
        smoothstep(0.0, 0.35, rimGlass)
        * smoothstep(0.7, 1.0, out_uv.y);

    glassBody += vec3(1.0)
        * topEdge
        * 0.045;

    float thickness = smoothstep(0.0, 0.5, rimGlass);

    glassBody *= mix(1.0, 0.96, thickness);

    float innerBevel = rimGlass * smoothA;

    glassBody *= 1.0 - 0.035 * innerBevel;

    glassBody += (
        hash21(gl_FragCoord.xy) - 0.5
    ) * 0.010 * frostAmount;

    fragColor = vec4(
        clamp(glassBody, 0.0, 1.0),
        glassOpacity
    );
}