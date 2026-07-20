/*{
  "CREDIT": "ossia team",
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "DESCRIPTION": "GGX prefiltered-environment cubemap for specular IBL. One shader instance writes every mip level of the 6-face prefiltered array: the runtime's EXECUTION_MODEL=PER_MIP loops the pass once per level of the target, binding the matching mip as the render target and exposing the level index as PASSINDEX. Combined with MULTIVIEW:6 one draw per mip covers all six faces via gl_ViewIndex. Output is a 2D TextureArray (QRhi's multiview + cubemap is mutually exclusive, qrhi.cpp:2561); downstream IBL consumers bind it as sampler2DArray and pick the face via the direction-to-face helper.",
  "CATEGORIES": ["3D", "IBL", "PBR"],

  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" },
    { "TYPE": "int",  "NAME": "v_face" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" },
    { "TYPE": "int",  "NAME": "v_face" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],

  "OUTPUTS": [
    { "NAME": "prefiltered", "TYPE": "color", "FORMAT": "rgba16f",
      "LAYERS": 6, "CUBEMAP": true,
      "WIDTH": 256, "HEIGHT": 256 }
  ],
  "MULTIVIEW": 6,
  "EXECUTION_MODEL": { "TYPE": "PER_MIP", "TARGET": "prefiltered" },

  "INPUTS": [
    { "NAME": "environment", "TYPE": "cubemap", "VISIBILITY": "fragment" },
    { "NAME": "mip_count", "TYPE": "long",
      "MIN": 1, "MAX": 16, "DEFAULT": 6,
      "LABEL": "Mip levels" },
    { "NAME": "samples", "TYPE": "long",
      "MIN": 64, "MAX": 4096, "DEFAULT": 1024,
      "LABEL": "Importance samples" },
    { "NAME": "env_resolution", "TYPE": "float",
      "MIN": 64.0, "MAX": 4096.0, "DEFAULT": 1024.0,
      "LABEL": "Source resolution" }
  ],

  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/

// Vertex stage lives in prefilter_ggx.vert — fullscreen triangle fired
// 6× via MULTIVIEW=6, wiring gl_ViewIndex → v_face so each face gets
// rasterised into its own layer of the prefiltered output array.

const float PI = 3.14159265358979323846;

// Same convention as irradiance_convolve.frag — see the comment
// there for the xy.y-sign rationale (clipSpaceCorrMatrix + sampled-
// UV top-at-y=0 orientation).
vec3 cubeFaceDir(int face, vec2 uv)
{
    vec2 xy = uv * 2.0 - 1.0;
    vec3 d;
    if      (face == 0) d = vec3( 1.0,  xy.y, -xy.x);
    else if (face == 1) d = vec3(-1.0,  xy.y,  xy.x);
    else if (face == 2) d = vec3( xy.x,  1.0, -xy.y);
    else if (face == 3) d = vec3( xy.x, -1.0,  xy.y);
    else if (face == 4) d = vec3( xy.x,  xy.y,  1.0);
    else                d = vec3(-xy.x,  xy.y, -1.0);
    return normalize(d);
}

float VanDerCorpus(uint n, uint base)
{
    float invBase = 1.0 / float(base);
    float denom   = 1.0;
    float result  = 0.0;
    for (uint i = 0u; i < 32u; ++i)
    {
        if (n > 0u)
        {
            denom   = mod(float(n), 2.0);
            result += denom * invBase;
            invBase = invBase / 2.0;
            n       = uint(float(n) / 2.0);
        }
    }
    return result;
}
vec2 Hammersley(uint i, uint N) { return vec2(float(i)/float(N), VanDerCorpus(i,2u)); }

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float a)
{
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 T  = normalize(cross(up, N));
    vec3 B  = cross(N, T);
    return normalize(T * H.x + B * H.y + N * H.z);
}

float D_GGX(float NdotH, float a)
{
    float a2 = a * a;
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

void main()
{
    vec3 N = cubeFaceDir(v_face, v_uv);
    vec3 R = N;
    vec3 V = R;

    // PASSINDEX is the current mip level (0..mip_count-1); the runtime
    // loops the shader once per mip in PER_MIP execution mode, binding
    // the matching texture level as the render target. Roughness maps
    // linearly across the mip chain so mip 0 stays mirror-sharp and the
    // coarsest mip lands at a = 1.
    float roughness = (mip_count <= 1) ? 0.0
                        : clamp(float(PASSINDEX) / float(mip_count - 1), 0.0, 1.0);
    float a = roughness * roughness;
    const uint SAMPLES = uint(samples);

    vec3 prefiltered = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0u; i < SAMPLES; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLES);
        vec3 H  = ImportanceSampleGGX(Xi, N, a);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            // Filtered importance sampling (Karis): pick a LOD in the
            // source cube that corresponds to the solid angle this
            // sample covers, reducing aliasing.
            float NdotH = max(dot(N, H), 0.0);
            float pdf = D_GGX(NdotH, a) * NdotH / (4.0 * max(dot(H, V), 1e-6)) + 1e-4;
            float saTexel  = 4.0 * PI / (6.0 * env_resolution * env_resolution);
            float saSample = 1.0 / (float(SAMPLES) * pdf + 1e-4);
            float mipLevel = (roughness == 0.0)
                             ? 0.0
                             : 0.5 * log2(saSample / saTexel);

            prefiltered += textureLod(environment, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    isf_FragColor = vec4(prefiltered / max(totalWeight, 1e-6), 1.0);
}
