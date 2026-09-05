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
#define MAX_STEPS 128
#define LIGHT_STEPS 4
#define EPSILON 1e-6
#define MAX_RAY_DISTANCE 2000.0
#define SIGMA_T 1.0 // those who know
#define ALPHA_DEPTH_THRESHOLD 0.9

float cloud_density(vec3 pos, float lod) {
    vec2 windOffset = wind_direction.xz * time * 0.0005;
    
    vec2 uvCoarse = (pos.xz * 0.0002) + windOffset;
    vec4 coarseData = textureLod(cloudMap, uvCoarse, lod);


    float cloudMask = coarseData.r;
    float cloudHeight = coarseData.g * (CLOUD_TOP - CLOUD_BOTTOM) + CLOUD_BOTTOM;
    float heightRange = 3.0 + coarseData.b * 5.0;

    float heightWeight = 1.0 - abs(pos.y - cloudHeight) / heightRange;
    heightWeight = clamp(heightWeight, 0.0, 1.0);
    heightWeight = heightWeight * heightWeight * (3.0 - 2.0 * heightWeight);

    float baseDensity = cloudMask * heightWeight;

    vec2 uvDetail = (-pos.xz * 0.001) + windOffset * 5.0;
    float detail = textureLod(cloudMap, uvDetail, lod + 2.0).r;
    detail = clamp(detail, 0.0, 1.0);

    return baseDensity * mix(0.4, 1.0, detail);
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

vec3 compute_normal(vec3 pos, float currentDensity) {
    float eps = 1.0;
    float dX = cloud_density(pos + vec3(eps, 0.0, 0.0), 0.5);
    float dZ = cloud_density(pos + vec3(0.0, 0.0, eps), 0.5);
    vec3 grad = vec3(dX - currentDensity, 0.0, dZ - currentDensity);
    grad.y = -0.1;
    float len = length(grad);
    if (len < 1e-6) return vec3(0.0, 1.0, 0.0);
    return normalize(-grad);
}

struct MarchResult {
    vec3 color;
    float alpha;
    float depth;
};

MarchResult ray_march_lighting(vec3 ro, vec3 rd, float tMin, float tMax) {
    MarchResult res;
    res.color = vec3(0.0);
    res.alpha = 0.0;
    res.depth = tMax;

    float totalDist = tMax - tMin;
    if (totalDist <= 0.0) return res;

    float verticalComponent = abs(rd.y);
    float adaptiveStepSize;
    
    if (verticalComponent < 0.001) {
        adaptiveStepSize = 0.5;
    } else {
        float verticalThickness = CLOUD_TOP - CLOUD_BOTTOM;
        float desiredVerticalSamples = 20.0;
        float stepSizeFromVertical = verticalThickness / (desiredVerticalSamples * verticalComponent);
        
        float stepSizeFromTotal = totalDist / float(MAX_STEPS);
        
        adaptiveStepSize = min(stepSizeFromVertical, stepSizeFromTotal);
        
        adaptiveStepSize = clamp(adaptiveStepSize, 0.5, 10.0);
    }

    int numSteps = int(ceil(totalDist / adaptiveStepSize));
    numSteps = min(numSteps, 512);
    
    float stepSize = totalDist / float(numSteps);
    
    float t = tMin;
    float transmittance = 1.0;
    vec3 ambient = ambientColor * 0.5;
    bool depthSet = false;

    for (int i = 0; i < numSteps; i++) {
        vec3 pos = ro + rd * t;
        float density = cloud_density(pos, 0.0);

        if (density < 0.001) {
            float emptyStep = min(stepSize * 2.0, 2.0);
            t += emptyStep;
            if (t > tMax) break;
            continue;
        }

        float sigma_t = density * SIGMA_T;
        float lightT = light_transmittance(pos);

        vec3 N = compute_normal(pos, density);
        float diffuse = max(dot(N, sunDirection), 0.0);

        vec3 sunContrib = sunColor * diffuse * lightT * sigma_t * stepSize;
        res.color += transmittance * (sunContrib + ambient * sigma_t * stepSize);

        float alphaStep = 1.0 - exp(-sigma_t * stepSize);
        res.alpha += transmittance * alphaStep;
        transmittance *= exp(-sigma_t * stepSize);

        if (!depthSet && res.alpha >= ALPHA_DEPTH_THRESHOLD) {
            res.depth = t;
            depthSet = true;
        }

        if (transmittance < 0.05 || res.alpha > 0.98) break;

        t += stepSize;
        if (t > tMax) break;
    }

    res.alpha = clamp(res.alpha, 0.0, 1.0);
    return res;
}

vec2 intersectCloudLayer(vec3 ro, vec3 rd) {
    if (abs(rd.y) < EPSILON) {
        if (ro.y >= CLOUD_BOTTOM && ro.y <= CLOUD_TOP) {
            return vec2(0.0, MAX_RAY_DISTANCE);
        } else {
            return vec2(0.0, -1.0);
        }
    }

    float t1 = (CLOUD_BOTTOM - ro.y) / rd.y;
    float t2 = (CLOUD_TOP - ro.y) / rd.y;

    float tMin = min(t1, t2);
    float tMax = max(t1, t2);

    tMin = max(tMin, 0.0);
    
    float effectiveMaxDist = MAX_RAY_DISTANCE;
    
    if (abs(rd.y) < 0.01) {
        effectiveMaxDist = min(MAX_RAY_DISTANCE, 500.0);
    }
    
    tMax = min(tMax, effectiveMaxDist);

    return vec2(tMin, tMax);
}

void main() {
    vec3 ro = cameraPos;

    vec2 uv = gl_FragCoord.xy / screenSize;
    vec4 ndc = vec4(uv * 2.0 - 1.0, -1.0, 1.0);
    vec4 viewPos = inverse(projection) * ndc;
    vec3 viewDir = normalize(viewPos.xyz / viewPos.w);
    vec3 rd = (inverse(view) * vec4(viewDir, 0.0)).xyz;

    vec2 tBounds = intersectCloudLayer(ro, rd);
    if (tBounds.x < tBounds.y) {
        MarchResult res = ray_march_lighting(ro, rd, tBounds.x, tBounds.y);

        if (res.alpha > 0.01) {
            gAlbedo = vec4(res.color, res.alpha);

            float tDepth = max(res.depth, 1e-4);
            vec3 cloudWorldPos = ro + rd * tDepth;
            vec4 clipPos = projection * view * vec4(cloudWorldPos, 1.0);
            float ndcZ = clipPos.z / clipPos.w;
            gDepth = clamp(ndcZ * 0.5 + 0.5, 0.0, 1.0);
        } else {
            gAlbedo = vec4(0.0);
            gDepth = 1.0;
        }
    } else {
        gAlbedo = vec4(0.0);
        gDepth = 1.0;
    }
}