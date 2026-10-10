
#version 330 core

in vec2 out_uv;

uniform sampler2D cloudMap;
uniform mat4 projection;
uniform mat4 view;
uniform vec3 cameraPos;
uniform vec2 screenSize;

uniform vec3 wind_direction;
uniform float time;

uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform vec3 ambientColor;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out float gDepth;

#define CLOUD_BOTTOM 80.0
#define CLOUD_TOP 90.0
#define CLOUD_VOLUME_BOTTOM (CLOUD_BOTTOM - 8.0)
#define CLOUD_VOLUME_TOP (CLOUD_TOP + 8.0)
#define MAX_STEPS 128
#define LIGHT_STEPS 4
#define EPSILON 1e-6
#define MAX_RAY_DISTANCE 2000.0
#define SIGMA_T 1.0 // those who know
#define ALPHA_DEPTH_THRESHOLD 0.9
#define NEAR_STEP_BIAS 0.65

float height_weight(float y, float center, float radius) {
    float weight = clamp(
        1.0 - abs(y - center) / radius,
        0.0,
        1.0
    );

    return weight * weight * (3.0 - 2.0 * weight);
}

float height_primitive(float x) {
    x = clamp(x, -1.0, 1.0);

    float x2 = x * x;

    return x - x * x2 + 0.5 * x * x2 * abs(x);
}

float height_weight_segment(
    float y0,
    float y1,
    float center,
    float radius
) {
    float a = (y0 - center) / radius;
    float b = (y1 - center) / radius;

    float delta = b - a;

    if (abs(delta) < 0.001) {
        return height_weight(
            0.5 * (y0 + y1),
            center,
            radius
        );
    }

    float integral =
        height_primitive(b) -
        height_primitive(a);

    return clamp(integral / delta, 0.0, 1.0);
}

float cloud_density(vec3 pos, float lod) {
    vec2 windOffset = wind_direction.xz * time * 0.0005;

    vec2 uvCoarse = pos.xz * 0.0002 + windOffset;
    vec4 coarseData = textureLod(cloudMap, uvCoarse, lod);

    float cloudMask = coarseData.r;

    float cloudHeight =
        coarseData.g * (CLOUD_TOP - CLOUD_BOTTOM) +
        CLOUD_BOTTOM;

    float heightRange = 3.0 + coarseData.b * 5.0;

    float heightWeight = height_weight(
        pos.y,
        cloudHeight,
        heightRange
    );

    float baseDensity = cloudMask * heightWeight;

    if (baseDensity < 0.001) {
        return 0.0;
    }

    vec2 uvDetail = -pos.xz * 0.001 + windOffset * 5.0;

    float detail = textureLod(
        cloudMap,
        uvDetail,
        lod + 2.0
    ).r;

    return baseDensity * mix(
        0.4,
        1.0,
        clamp(detail, 0.0, 1.0)
    );
}

float cloud_density_segment(
    vec3 p0,
    vec3 p1,
    float lod,
    out float centerDensity
) {
    vec3 pos = 0.5 * (p0 + p1);

    vec2 windOffset = wind_direction.xz * time * 0.0005;

    vec2 uvCoarse = pos.xz * 0.0002 + windOffset;
    vec4 coarseData = textureLod(cloudMap, uvCoarse, lod);

    float cloudMask = coarseData.r;

    float cloudHeight =
        coarseData.g * (CLOUD_TOP - CLOUD_BOTTOM) +
        CLOUD_BOTTOM;

    float heightRange = 3.0 + coarseData.b * 5.0;

    float averageHeight = height_weight_segment(
        p0.y,
        p1.y,
        cloudHeight,
        heightRange
    );

    float baseDensity = cloudMask * averageHeight;

    if (baseDensity < 0.001) {
        centerDensity = 0.0;
        return 0.0;
    }

    vec2 uvDetail = -pos.xz * 0.001 + windOffset * 5.0;

    float detail = textureLod(
        cloudMap,
        uvDetail,
        lod + 2.0
    ).r;

    float detailFactor = mix(
        0.4,
        1.0,
        clamp(detail, 0.0, 1.0)
    );

    float centerHeight = height_weight(
        pos.y,
        cloudHeight,
        heightRange
    );

    centerDensity = cloudMask * centerHeight * detailFactor;

    return baseDensity * detailFactor;
}

float light_transmittance(vec3 pos) {
    float tau = 0.0;
    float lightStep = 2.5;

    for (int i = 0; i < LIGHT_STEPS; i++) {
        vec3 p = pos + sunDirection * (float(i) + 0.5) * lightStep;

        tau += cloud_density(p, 1.0) * SIGMA_T * lightStep;
    }

    return exp(-tau);
}

vec3 compute_normal(vec3 pos, float currentDensity, float lod) {
    float eps = 1.0;

    float dX = cloud_density(
        pos + vec3(eps, 0.0, 0.0),
        lod
    );

    float dZ = cloud_density(
        pos + vec3(0.0, 0.0, eps),
        lod
    );

    vec3 grad = vec3(
        dX - currentDensity,
        -0.1,
        dZ - currentDensity
    );

    float len = length(grad);

    if (len < 1e-6) {
        return vec3(0.0, 1.0, 0.0);
    }

    return normalize(-grad);
}

struct MarchResult {
    vec3 color;
    float alpha;
    float depth;
};

float ray_distribution(float u) {
    return u * (1.0 - NEAR_STEP_BIAS) +
           u * u * NEAR_STEP_BIAS;
}

MarchResult ray_march_lighting(
    vec3 ro,
    vec3 rd,
    float tMin,
    float tMax
) {
    MarchResult res;
    res.color = vec3(0.0);
    res.alpha = 0.0;
    res.depth = tMax;

    float totalDist = tMax - tMin;

    if (totalDist <= 0.0) {
        return res;
    }

    float verticalComponent = max(abs(rd.y), 0.001);

    float verticalThickness =
        CLOUD_VOLUME_TOP - CLOUD_VOLUME_BOTTOM;

    float desiredStepSize =
        verticalThickness / (40.0 * verticalComponent);

    desiredStepSize = clamp(
        desiredStepSize,
        0.5,
        10.0
    );

    int numSteps = clamp(
        int(ceil(totalDist / desiredStepSize)),
        1,
        MAX_STEPS
    );

    float invSteps = 1.0 / float(numSteps);

    vec2 mapSize = vec2(textureSize(cloudMap, 0));

    float textureScale =
        0.0002 * max(mapSize.x, mapSize.y);

    float horizontalComponent = length(rd.xz);

    float transmittance = 1.0;
    float weightedDepth = 0.0;
    float accumulatedWeight = 0.0;

    vec3 ambient = ambientColor * 0.5;

    float previousT = tMin;

    for (int i = 0; i < MAX_STEPS; i++) {
        if (i >= numSteps) {
            break;
        }

        float u = float(i + 1) * invSteps;

        float tEnd = tMin +
            totalDist * ray_distribution(u);

        float tStart = previousT;
        previousT = tEnd;

        float segmentLength = tEnd - tStart;

        if (segmentLength <= 0.0) {
            continue;
        }

        float tMid = 0.5 * (tStart + tEnd);

        vec3 p0 = ro + rd * tStart;
        vec3 p1 = ro + rd * tEnd;
        vec3 pos = ro + rd * tMid;

        float texelFootprint =
            segmentLength *
            horizontalComponent *
            textureScale;

        float lod = max(
            log2(max(texelFootprint, 1.0)),
            0.0
        );

        float centerDensity = 0.0;

        float density = cloud_density_segment(
            p0,
            p1,
            lod,
            centerDensity
        );

        if (density < 0.001) {
            continue;
        }

        float sigma_t = density * SIGMA_T;

        float opticalDepth = sigma_t * segmentLength;

        float segmentTransmittance = exp(-opticalDepth);
        float alphaStep = 1.0 - segmentTransmittance;

        float weight = transmittance * alphaStep;

        if (weight > 0.0001) {
            float lightT = light_transmittance(pos);

            vec3 N = compute_normal(
                pos,
                centerDensity,
                max(lod, 0.5)
            );

            float diffuse = max(
                dot(N, sunDirection),
                0.0
            );

            vec3 lighting =
                ambient +
                sunColor * diffuse * lightT;

            res.color += lighting * weight;
        }

        float depthOffset;

        if (opticalDepth < 0.01) {
            depthOffset = segmentLength * 0.5;
        } else {
            depthOffset =
                1.0 / sigma_t -
                segmentLength *
                segmentTransmittance /
                max(alphaStep, 1e-6);
        }

        depthOffset = clamp(
            depthOffset,
            0.0,
            segmentLength
        );

        weightedDepth +=
            (tStart + depthOffset) * weight;

        accumulatedWeight += weight;

        transmittance *= segmentTransmittance;

        if (transmittance < 0.02) {
            break;
        }
    }

    res.alpha = clamp(
        1.0 - transmittance,
        0.0,
        1.0
    );

    if (accumulatedWeight > 1e-6) {
        res.depth = weightedDepth / accumulatedWeight;
    }

    return res;
}

vec2 intersectCloudLayer(vec3 ro, vec3 rd) {
    if (abs(rd.y) < EPSILON) {
        if (
            ro.y >= CLOUD_VOLUME_BOTTOM &&
            ro.y <= CLOUD_VOLUME_TOP
        ) {
            return vec2(0.0, MAX_RAY_DISTANCE);
        }

        return vec2(0.0, -1.0);
    }

    float t1 = (CLOUD_VOLUME_BOTTOM - ro.y) / rd.y;
    float t2 = (CLOUD_VOLUME_TOP - ro.y) / rd.y;

    float tMin = max(min(t1, t2), 0.0);
    float tMax = min(max(t1, t2), MAX_RAY_DISTANCE);

    return vec2(tMin, tMax);
}

void main() {
    vec3 ro = cameraPos;

    vec2 uv = gl_FragCoord.xy / screenSize;
    vec2 ndc = uv * 2.0 - 1.0;

    vec2 viewXY = (
        ndc + vec2(projection[2][0], projection[2][1])
    ) / vec2(projection[0][0], projection[1][1]);

    vec3 viewDir = normalize(vec3(viewXY, -1.0));

    vec3 rd = normalize(
        transpose(mat3(view)) * viewDir
    );

    vec2 tBounds = intersectCloudLayer(ro, rd);

    if (tBounds.x < tBounds.y) {
        MarchResult res = ray_march_lighting(
            ro,
            rd,
            tBounds.x,
            tBounds.y
        );

        if (res.alpha > 0.0001) {
            gAlbedo = vec4(res.color, res.alpha);

            float tDepth = max(res.depth, 1e-4);
            vec3 cloudWorldPos = ro + rd * tDepth;

            vec4 clipPos =
                projection * view *
                vec4(cloudWorldPos, 1.0);

            float ndcZ = clipPos.z / clipPos.w;

            gDepth = clamp(
                ndcZ * 0.5 + 0.5,
                0.0,
                1.0
            );
        } else {
            gAlbedo = vec4(res.color, res.alpha);
            gDepth = 1.0;
        }
    } else {
        gAlbedo = vec4(0.0);
        gDepth = 1.0;
    }
}
