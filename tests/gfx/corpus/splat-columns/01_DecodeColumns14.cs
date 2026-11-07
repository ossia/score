/*{
  "DESCRIPTION": "Performance variant of 01_DecodeColumns.cs declaring the 14 columns of SH degree 0 only. 3DGS by column name, stage 01: decode + global depth sort. Reads every gaussian layout by its column names (COLUMNS), wherever and however the file stores them: the INRIA export (62 floats), exports without normals (Postshot, cluster.fly: 59), SH degree 0 to 3 (f_rest_* count 0, 9, 24 or 45, channel-major), double or integer columns, reordered or extra columns, antimatter15 .splat (linear scale_linear_*, 8-bit red/green/blue/alpha and rot_u8_*) and .spz. Nothing is copied: the rows stay in the Scene Preprocessor's buffer. PASS 0 decodes each splat as 01_Decode (Kerbl 2023 computeColorFromSH, sigmoid opacity, exp scale, PLY quaternion order w,x,y,z) up to the SH degree the file has, composes the cloud_meta model matrix and writes a view-depth sort key; PASSES 1..12 radix-sort the keys; PASS 13 gathers the 6-vertex x N-instance quads in back to front order for 02_DrawSplat.",
  "CREDIT": "Kerbl, Kopanas, Leimkuehler, Drettakis, 3D Gaussian Splatting for Real-Time Radiance Field Rendering, ACM ToG 42(4) 2023; reference rasteriser graphdeco-inria/diff-gaussian-rasterization (forward.cu computeColorFromSH). Radix sort: Merrill & Grimshaw 2011 LSD structure (histogram / scan / stable scatter). ossia score port.",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": [
    "3D",
    "Splat"
  ],
  "TYPES": [
    {
      "NAME": "CloudMeta",
      "LAYOUT": [
        {"NAME": "model", "TYPE": "mat4"},
        {"NAME": "bounds_min", "TYPE": "vec4"},
        {"NAME": "bounds_max", "TYPE": "vec4"},
        {"NAME": "primitive_offset", "TYPE": "uint"},
        {"NAME": "primitive_count", "TYPE": "uint"},
        {"NAME": "transform_slot", "TYPE": "uint"},
        {"NAME": "format_param_index", "TYPE": "uint"},
        {"NAME": "_pad0", "TYPE": "uvec4"}
      ]
    },
    {
      "NAME": "DecodedSplat",
      "LAYOUT": [
        {"NAME": "pos", "TYPE": "vec4"},
        {"NAME": "color", "TYPE": "vec4"},
        {"NAME": "scale", "TYPE": "vec4"},
        {"NAME": "rotation", "TYPE": "vec4"}
      ]
    }
  ],
  "RESOURCES": [
    {"NAME": "shDegree", "TYPE": "long", "DEFAULT": 3, "MIN": 0, "MAX": 3, "LABEL": "SH degree (0 = flat colour, 3 = full view dependence)"},
    {"NAME": "scaleMod", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.01, "MAX": 10.0, "LABEL": "Splat size multiplier"},
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "TOPOLOGY": "triangles",
      "VERTEX_COUNT": 6,
      "INSTANCE_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [
        {"NAME": "pos", "SEMANTIC": "position", "TYPE": "vec3", "ACCESS": "write_only", "RATE": "instance"},
        {"NAME": "color", "SEMANTIC": "color", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "instance"},
        {"NAME": "scale", "SEMANTIC": "scale", "TYPE": "vec3", "ACCESS": "write_only", "RATE": "instance"},
        {"NAME": "rotation", "SEMANTIC": "rotation", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "instance"}
      ],
      "AUXILIARY": [
        {
          "NAME": "camera",
          "COPY_FROM": {"GEOMETRY": "geoIn"}
        },
        {
          "NAME": "gs_decoded",
          "ACCESS": "read_write",
          "SIZE": "$VERTEX_COUNT_geoIn",
          "LAYOUT": [
            {
              "NAME": "values",
              "TYPE": "DecodedSplat[]"
            }
          ]
        },
        {
          "NAME": "gs_keys_a",
          "ACCESS": "read_write",
          "SIZE": "$VERTEX_COUNT_geoIn",
          "LAYOUT": [
            {
              "NAME": "values",
              "TYPE": "uint[]"
            }
          ]
        },
        {
          "NAME": "gs_keys_b",
          "ACCESS": "read_write",
          "SIZE": "$VERTEX_COUNT_geoIn",
          "LAYOUT": [
            {
              "NAME": "values",
              "TYPE": "uint[]"
            }
          ]
        },
        {
          "NAME": "gs_vals_a",
          "ACCESS": "read_write",
          "SIZE": "$VERTEX_COUNT_geoIn",
          "LAYOUT": [
            {
              "NAME": "values",
              "TYPE": "uint[]"
            }
          ]
        },
        {
          "NAME": "gs_vals_b",
          "ACCESS": "read_write",
          "SIZE": "$VERTEX_COUNT_geoIn",
          "LAYOUT": [
            {
              "NAME": "values",
              "TYPE": "uint[]"
            }
          ]
        },
        {
          "NAME": "gs_hist",
          "ACCESS": "read_write",
          "SIZE": "$VERTEX_COUNT_geoIn + 256",
          "LAYOUT": [
            {
              "NAME": "values",
              "TYPE": "uint[]"
            }
          ]
        }
      ]
    },
    {
      "NAME": "geoIn",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        {"NAME": "cloud_id", "SEMANTIC": "custom", "TYPE": "uint", "ACCESS": "read_only"}
      ],
      "AUXILIARY": [
        {
          "NAME": "cloud_meta",
          "ACCESS": "read_only",
          "LAYOUT": [
            {
              "NAME": "entries",
              "TYPE": "CloudMeta[]"
            }
          ]
        },
        {
          "NAME": "camera",
          "TYPE": "uniform",
          "LAYOUT": [
            {"NAME": "view", "TYPE": "mat4"},
            {"NAME": "projection", "TYPE": "mat4"},
            {"NAME": "viewProjection", "TYPE": "mat4"},
            {"NAME": "cameraPosition", "TYPE": "vec4"},
            {"NAME": "renderSize", "TYPE": "vec4"},
            {"NAME": "params", "TYPE": "vec4"}
          ]
        }
      ],
      "COLUMNS": [
        "x", "y", "z", "f_dc_0", "f_dc_1", "f_dc_2", "opacity", "scale_0", "scale_1",
        "scale_2", "rot_0", "rot_1", "rot_2", "rot_3"
      ]
    },
    {"NAME": "depthSort", "TYPE": "bool", "DEFAULT": true, "LABEL": "Depth sort (off = file order, for A/B)"}
  ],
  "PASSES": [
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "PER_VERTEX", "TARGET": "geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {
        "TYPE": "MANUAL",
        "WORKGROUPS": [
          1,
          1,
          1
        ]
      }
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {
        "TYPE": "MANUAL",
        "WORKGROUPS": [
          1,
          1,
          1
        ]
      }
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {
        "TYPE": "MANUAL",
        "WORKGROUPS": [
          1,
          1,
          1
        ]
      }
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {
        "TYPE": "MANUAL",
        "WORKGROUPS": [
          1,
          1,
          1
        ]
      }
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    },
    {
      "LOCAL_SIZE": [
        256,
        1,
        1
      ],
      "EXECUTION_MODEL": {"TYPE": "1D_BUFFER", "TARGET": "$VERTEX_COUNT_geoIn"}
    }
  ]
}*/

// Spherical harmonics constants: graphdeco-inria/diff-gaussian-rasterization
// cuda_rasterizer/forward.cu (SH_C0..SH_C3).
const float SH_C0 = 0.28209479177387814;
const float SH_C1 = 0.4886025119029199;
const float SH_C2[5] = float[5](
    1.0925484305920792, -1.0925484305920792,
    0.31539156525252005, -1.0925484305920792, 0.5462742152960396);
const float SH_C3[7] = float[7](
   -0.5900435899266435,  2.890611442640554, -0.4570457994644658,
    0.3731763325901154, -0.4570457994644658, 1.445305721320277,
   -0.5900435899266435);

// A column of row i, by name (0 when the file has no such column).
#define COL(name, i) ISF_COLUMN(geoIn, name, i)
#define HAS(name) ISF_HAS_COLUMN(geoIn, name)

// Rotation matrix -> quaternion (x,y,z,w), Shepperd's method.
vec4 matToQuat(mat3 m)
{
    // m[c][r] (GLSL column-major)
    float tr = m[0][0] + m[1][1] + m[2][2];
    vec4 q;
    if (tr > 0.0) {
        float s = sqrt(tr + 1.0) * 2.0;
        q = vec4((m[1][2] - m[2][1]) / s, (m[2][0] - m[0][2]) / s, (m[0][1] - m[1][0]) / s, 0.25 * s);
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        float s = sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
        q = vec4(0.25 * s, (m[1][0] + m[0][1]) / s, (m[2][0] + m[0][2]) / s, (m[1][2] - m[2][1]) / s);
    } else if (m[1][1] > m[2][2]) {
        float s = sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
        q = vec4((m[1][0] + m[0][1]) / s, 0.25 * s, (m[2][1] + m[1][2]) / s, (m[2][0] - m[0][2]) / s);
    } else {
        float s = sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
        q = vec4((m[2][0] + m[0][2]) / s, (m[2][1] + m[1][2]) / s, 0.25 * s, (m[0][1] - m[1][0]) / s);
    }
    return normalize(q);
}

// Hamilton product, both (x,y,z,w).
vec4 quatMul(vec4 a, vec4 b)
{
    return vec4(a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz),
                a.w * b.w - dot(a.xyz, b.xyz));
}

shared uint s_hist[256];
shared uint s_digits[256];
shared uint s_prefix[256];

void decodePass(uint idx, uint N)
{
    // The columns first.
    vec3 p = vec3(COL(x, idx), COL(y, idx), COL(z, idx));
    float a = 1.0 / (1.0 + exp(-COL(opacity, idx)));
    vec3 s3 = exp(vec3(COL(scale_0, idx), COL(scale_1, idx), COL(scale_2, idx)));
    // Quaternion in PLY order (w,x,y,z).
    vec4 wxyz = vec4(COL(rot_0, idx), COL(rot_1, idx), COL(rot_2, idx), COL(rot_3, idx));
    vec3 dc = vec3(COL(f_dc_0, idx), COL(f_dc_1, idx), COL(f_dc_2, idx));

    uint cid = ISF_READ(geoIn, cloud_id)[idx];
    mat4 model = cloud_meta.entries[cid].model;
    vec3 posWorld = (model * vec4(p, 1.0)).xyz;

    // Model linear part = Rm * uniform scale (non-uniform scale is not
    // representable as a quaternion + per-axis scale; its mean is used).
    mat3 L = mat3(model);
    vec3 colLen = vec3(length(L[0]), length(L[1]), length(L[2]));
    mat3 Rm = mat3(L[0] / max(colLen.x, 1e-20), L[1] / max(colLen.y, 1e-20), L[2] / max(colLen.z, 1e-20));
    float mScale = (colLen.x + colLen.y + colLen.z) / 3.0;

    // SH degree 0: the DC term only.
    vec3 rgb = max(SH_C0 * dc + 0.5, vec3(0.0));

    vec3 sc = s3 * scaleMod * mScale;
    // (w,x,y,z) to (x,y,z,w); pre-compose the cloud rotation.
    vec4 q = normalize(wxyz.yzwx);
    vec4 qw = quatMul(matToQuat(Rm), q);

    gs_decoded.values[idx].pos      = vec4(posWorld, 1.0);
    gs_decoded.values[idx].color    = vec4(rgb, a);
    gs_decoded.values[idx].scale    = vec4(sc, 0.0);
    gs_decoded.values[idx].rotation = qw;

    // Sort key: view-space depth (GL view looks down -z). Far first.
    float depth = -(camera.view * vec4(posWorld, 1.0)).z;
    uint key = (depth > 0.0) ? ~floatBitsToUint(depth) : 0xFFFFFFFFu;
    gs_keys_a.values[idx] = key;
    gs_vals_a.values[idx] = idx;
}

void main()
{
    uint gid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;
    uint wg  = gl_WorkGroupID.x;
    // The primitive count. NOT ISF_READ(geoIn, cloud_id).length(): the Scene
    // Preprocessor binds its bucket buffers (raw_splats, cloud_id_lookup) with
    // their grown CAPACITY, so .length() over-counts, and the extra zero keys
    // corrupt the radix histogram. gs_keys_a is sized exactly
    // $VERTEX_COUNT_geoIn.
    uint N   = uint(gs_keys_a.values.length());
    int  P   = int(PASSINDEX);

    if (P == 0) {
        if (gid < N) decodePass(gid, N);
        return;
    }

    if (P == 13) {
        if (gid >= N) return;
        uint src = depthSort ? gs_vals_a.values[gid] : gid;
        DecodedSplat d = gs_decoded.values[min(src, N - 1u)];
        ISF_WRITE(geoOut, pos)[gid]      = d.pos.xyz;
        ISF_WRITE(geoOut, color)[gid]    = d.color;
        ISF_WRITE(geoOut, scale)[gid]    = d.scale.xyz;
        ISF_WRITE(geoOut, rotation)[gid] = d.rotation;
        return;
    }

    if (!depthSort) return;

    // Radix passes: digit d = (P-1)/3, phase = (P-1)%3.
    int digitIdx = (P - 1) / 3;
    int phase    = (P - 1) % 3;
    uint shift   = uint(digitIdx) * 8u;
    bool fromA   = (digitIdx & 1) == 0;       // A->B, B->A, A->B, B->A
    uint numWG   = (N + 255u) / 256u;

    if (phase == 0) {
        // Histogram of this workgroup's 256 keys.
        s_hist[lid] = 0u;
        barrier();
        if (gid < N) {
            uint key = fromA ? gs_keys_a.values[gid] : gs_keys_b.values[gid];
            atomicAdd(s_hist[(key >> shift) & 0xFFu], 1u);
        }
        barrier();
        if (wg < numWG) gs_hist.values[wg * 256u + lid] = s_hist[lid];
    } else if (phase == 1) {
        // Single workgroup: per-digit total across workgroups, exclusive
        // prefix over digits, then per-(workgroup, digit) base offsets.
        uint digit = lid;
        uint total = 0u;
        for (uint w = 0u; w < numWG; ++w) total += gs_hist.values[w * 256u + digit];
        s_hist[digit] = total;
        barrier();
        if (digit == 0u) {
            uint run = 0u;
            for (uint d = 0u; d < 256u; ++d) { s_prefix[d] = run; run += s_hist[d]; }
        }
        barrier();
        uint run = s_prefix[digit];
        for (uint w = 0u; w < numWG; ++w) {
            uint slot = w * 256u + digit;
            uint v = gs_hist.values[slot];
            gs_hist.values[slot] = run;
            run += v;
        }
    } else {
        // Stable scatter: rank = number of lower-local-id threads of this
        // workgroup with the same digit.
        bool valid = gid < N;
        uint key = 0u, val = 0u, digit = 0xFFFFFFFFu;
        if (valid) {
            key = fromA ? gs_keys_a.values[gid] : gs_keys_b.values[gid];
            val = fromA ? gs_vals_a.values[gid] : gs_vals_b.values[gid];
            digit = (key >> shift) & 0xFFu;
        }
        s_digits[lid] = digit;
        barrier();
        if (!valid) return;
        uint rank = 0u;
        for (uint i = 0u; i < lid; ++i) rank += (s_digits[i] == digit) ? 1u : 0u;
        uint dst = gs_hist.values[wg * 256u + digit] + rank;
        if (dst < N) {
            if (fromA) { gs_keys_b.values[dst] = key; gs_vals_b.values[dst] = val; }
            else       { gs_keys_a.values[dst] = key; gs_vals_a.values[dst] = val; }
        }
    }
}
