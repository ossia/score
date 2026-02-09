#pragma once

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <ossia/detail/pod_vector.hpp>

// clang-format off
#if defined(near)
#undef near
#undef far
#endif
// clang-format on

namespace score::gfx
{

/**
 * @brief Gaussian Splat rendering node
 *
 * A full rendering node for 3D Gaussian Splatting.
 * Uses instanced quad rendering with EWA (Elliptical Weighted Average) projection.
 *
 * Pipeline (per frame):
 *   1. SH preprocess (compute): raw 256-byte splats → compact 64-byte splats
 *      Evaluates spherical harmonics, applies exp(scale), sigmoid(opacity)
 *   2. Depth key generation (compute): writes sortable uint keys
 *   3. Radix sort (compute): sorts indices back-to-front
 *   4. Render pass: instanced alpha-blended quads using sorted indices
 *
 * Input ports:
 *   - Raw Splat Buffer: GPU storage buffer, 256 bytes per splat
 *     (layout matches GaussianSplatData from Ply.hpp)
 *
 * Output ports:
 *   - Rendered image
 */
struct GaussianSplatNode : public NodeModel
{
public:
  GaussianSplatNode();
  virtual ~GaussianSplatNode();

  score::gfx::NodeRenderer* createRenderer(RenderList&) const noexcept override;
  void process(Message&& msg) override;

  int splatCount{};
  float scaleFactor{1.0f};
  bool enableSorting{true};
  uint32_t shDegree{3}; // 0, 1, 2, or 3

  // Model transform
  ossia::vec3f modelPosition{0.f, 0.f, 0.f};
  ossia::vec3f modelRotation{0.f, 0.f, 0.f}; // Euler angles in degrees (pitch, yaw, roll)
  ossia::vec3f modelScale{1.f, 1.f, 1.f};

  // Camera parameters
  ossia::vec3f position{-1.f, -1.f, -1.f};
  ossia::vec3f center{0.f, 0.f, 0.f};
  float fov{90.f};
  float near{0.001f};
  float far{10000.f};
};

/**
 * @brief Renderer for GaussianSplatNode
 *
 * Rendering pipeline:
 * 1. runInitialPasses: Compute depth keys and perform GPU radix sort
 * 2. runRenderPass: Draw sorted splats with alpha blending
 */
class GaussianSplatRenderer final : public score::gfx::GenericNodeRenderer
{
public:
  explicit GaussianSplatRenderer(const GaussianSplatNode& node);
  ~GaussianSplatRenderer();

  TextureRenderTarget renderTargetForInput(const Port& p) override;
  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;
  void runRenderPass(RenderList&, QRhiCommandBuffer& cb, Edge& edge) override;
  void release(RenderList&) override;

private:
  void createPreprocessPipeline(RenderList& renderer);
  void createRenderPipeline(RenderList& renderer);
  void createSortPipelines(RenderList& renderer);

  const GaussianSplatNode& m_node;

  // Input render target
  TextureRenderTarget m_inputTarget;

  // Render pipeline resources
  QRhiBuffer* m_uniformBuffer{};
  QRhiBuffer* m_dummyStorageBuffer{}; // Small buffer for unused bindings
  QRhiGraphicsPipeline* m_pipeline{};
  QRhiShaderResourceBindings* m_bindings{};

  // SH preprocessing compute resources
  // Converts raw 256-byte splats → compact 64-byte rendering splats
  QRhiBuffer* m_rawSplatBuffer{};    // Input: raw PLY data (256 bytes/splat)
  QRhiBuffer* m_renderSplatBuffer{}; // Output: compact (64 bytes/splat)
  QRhiBuffer* m_preprocessUniformBuffer{};
  QRhiComputePipeline* m_preprocessPipeline{};
  QRhiShaderResourceBindings* m_preprocessSrb{};

  // Sorting compute resources
  QRhiBuffer* m_sortKeysBuffer{};         // Depth keys (float -> uint for sorting)
  QRhiBuffer* m_sortKeysAltBuffer{};      // Double buffer for key ping-pong
  QRhiBuffer* m_sortIndicesBuffer{};      // Sorted indices
  QRhiBuffer* m_sortIndicesAltBuffer{};   // Double buffer for index ping-pong
  QRhiBuffer* m_histogramBuffer{};        // Histogram for radix sort
  QRhiBuffer* m_sortUniformBuffer{};      // Depth key pass uniforms
  QRhiBuffer* m_sortPassUniformBuffer{};  // Histogram/scatter/prefix uniforms
  QRhiBuffer* m_prefixSumUniformBuffer{}; // Prefix sum uniforms

  QRhiComputePipeline* m_depthKeyPipeline{};
  QRhiComputePipeline* m_histogramPipeline{};
  QRhiComputePipeline* m_prefixSumPipeline{};
  QRhiComputePipeline* m_sortPipeline{};

  QRhiShaderResourceBindings* m_depthKeySrb{};
  QRhiShaderResourceBindings* m_histogramSrb{};
  QRhiShaderResourceBindings* m_histogramSrbAlt{}; // For odd passes
  QRhiShaderResourceBindings* m_prefixSumSrb{};
  QRhiShaderResourceBindings* m_sortSrb{};
  QRhiShaderResourceBindings* m_sortSrbAlt{}; // For ping-pong

  ossia::small_vector<Sampler, 8> m_samplers;

  int64_t m_lastSplatCount{0};
  bool m_preprocessResourcesCreated{false};
  bool m_sortResourcesCreated{false};

  static constexpr int64_t MAX_SPLATS = 50000000;
  static constexpr int SORT_WORKGROUP_SIZE = 256;
  static constexpr int RADIX_BITS = 8;
  static constexpr int NUM_BUCKETS = 256; // 2^RADIX_BITS
};

// Shader sources
namespace GaussianSplatShaders
{

//=============================================================================
// COMPUTE SHADER: SH PREPROCESSING (raw 256B → compact 64B per splat)
//=============================================================================

/**
 * Compute shader: Preprocess raw Gaussian Splat data
 *
 * Reads raw 256-byte PLY splats and writes compact 64-byte rendering splats:
 *   - Evaluates spherical harmonics for view-dependent color
 *   - Applies exp() to log-space scale
 *   - Applies sigmoid() to raw opacity
 *   - Normalizes quaternion
 *   - Reorders rotation from (w,x,y,z) to (x,y,z,w) for the vertex shader
 */
static constexpr auto preprocess_shader = R"_(#version 450
layout(local_size_x = 256) in;

// Raw splat: 64 floats = 256 bytes (matches PLY loader output)
//   [0..2]   position (x,y,z)
//   [3..5]   normal (nx,ny,nz) — unused
//   [6..8]   SH DC (f_dc_0, f_dc_1, f_dc_2)
//   [9..53]  SH rest (f_rest_0 .. f_rest_44)
//   [54]     opacity (pre-sigmoid)
//   [55..57] scale (log-space)
//   [58..61] rotation (w,x,y,z)
//   [62..63] padding

layout(std430, binding = 0) readonly buffer RawSplatBuffer {
    float rawData[];  // 64 floats per splat
};

// Compact rendering splat: 16 floats = 64 bytes
//   vec4 position (xyz, 0)
//   vec4 scale    (xyz, 0)    — already exp'd
//   vec4 rotation (x,y,z,w)  — normalized
//   vec4 color    (r,g,b,a)  — SH evaluated, alpha = sigmoid(opacity)

struct RenderSplat {
    vec4 position;
    vec4 scale;
    vec4 rotation;
    vec4 color;
};

layout(std430, binding = 1) writeonly buffer RenderSplatBuffer {
    RenderSplat renderSplats[];
};

layout(std140, binding = 2) uniform Params {
    mat4 view;
    vec3 camPos;      // Camera position in world space
    uint splatCount;
    uint shDegree;    // 0, 1, 2, or 3
    float scaleMod;
    uint _pad0;
    uint _pad1;
};

// Spherical harmonics constants
const float SH_C0 = 0.28209479177387814;

const float SH_C1 = 0.4886025119029199;

const float SH_C2[5] = float[5](
    1.0925484305920792,
    -1.0925484305920792,
    0.31539156525252005,
    -1.0925484305920792,
    0.5462742152960396
);

const float SH_C3[7] = float[7](
    -0.5900435899266435,
    2.890611442640554,
    -0.4570457994644658,
    0.3731763325901154,
    -0.4570457994644658,
    1.445305721320277,
    -0.5900435899266435
);

vec3 evaluateSH(uint base, vec3 dir) {
    // Degree 0
    vec3 result = SH_C0 * vec3(
        rawData[base + 6],
        rawData[base + 7],
        rawData[base + 8]
    );

    if (shDegree < 1) {
        return result + 0.5;
    }

    // Degree 1
    float x = dir.x, y = dir.y, z = dir.z;

    // f_rest layout: [0..14] = R channel rest, [15..29] = G, [30..44] = B
    // But the INRIA convention interleaves: [0..2] = degree1 for R,G,B etc.
    // Actually the standard layout is:
    //   f_rest[0..14]:  coeffs 1..15 for channel 0 (R)
    //   f_rest[15..29]: coeffs 1..15 for channel 1 (G)
    //   f_rest[30..44]: coeffs 1..15 for channel 2 (B)

    uint r = base + 9;   // f_rest_0 start
    // Degree 1: 3 coefficients per channel, interleaved as RGB triplets
    // Coeff indices in f_rest: R=[0,1,2], G=[15,16,17], B=[30,31,32]
    result += SH_C1 * (
        - y * vec3(rawData[r+0],  rawData[r+15], rawData[r+30])
        + z * vec3(rawData[r+1],  rawData[r+16], rawData[r+31])
        - x * vec3(rawData[r+2],  rawData[r+17], rawData[r+32])
    );

    if (shDegree < 2) {
        return result + 0.5;
    }

    // Degree 2: 5 coefficients per channel
    // R=[3..7], G=[18..22], B=[33..37]
    float xx = x*x, yy = y*y, zz = z*z, xy = x*y, yz = y*z, xz = x*z;

    result += SH_C2[0] * xy       * vec3(rawData[r+3],  rawData[r+18], rawData[r+33]);
    result += SH_C2[1] * yz       * vec3(rawData[r+4],  rawData[r+19], rawData[r+34]);
    result += SH_C2[2] * (2.*zz - xx - yy)
                                   * vec3(rawData[r+5],  rawData[r+20], rawData[r+35]);
    result += SH_C2[3] * xz       * vec3(rawData[r+6],  rawData[r+21], rawData[r+36]);
    result += SH_C2[4] * (xx - yy)* vec3(rawData[r+7],  rawData[r+22], rawData[r+37]);

    if (shDegree < 3) {
        return result + 0.5;
    }

    // Degree 3: 7 coefficients per channel
    // R=[8..14], G=[23..29], B=[38..44]
    result += SH_C3[0] * y*(3.*xx - yy)
                                   * vec3(rawData[r+8],  rawData[r+23], rawData[r+38]);
    result += SH_C3[1] * xy*z     * vec3(rawData[r+9],  rawData[r+24], rawData[r+39]);
    result += SH_C3[2] * y*(4.*zz - xx - yy)
                                   * vec3(rawData[r+10], rawData[r+25], rawData[r+40]);
    result += SH_C3[3] * z*(2.*zz - 3.*xx - 3.*yy)
                                   * vec3(rawData[r+11], rawData[r+26], rawData[r+41]);
    result += SH_C3[4] * x*(4.*zz - xx - yy)
                                   * vec3(rawData[r+12], rawData[r+27], rawData[r+42]);
    result += SH_C3[5] * z*(xx - yy)
                                   * vec3(rawData[r+13], rawData[r+28], rawData[r+43]);
    result += SH_C3[6] * x*(xx - 3.*yy)
                                   * vec3(rawData[r+14], rawData[r+29], rawData[r+44]);

    return result + 0.5;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= splatCount) return;

    uint base = idx * 64; // 64 floats per raw splat

    // Position
    vec3 pos = vec3(rawData[base], rawData[base+1], rawData[base+2]);

    // View direction for SH evaluation (world space, from camera towards splat)
    // Must match the INRIA training convention: dir = pos - campos
    vec3 dir = normalize(pos - camPos);

    // Evaluate SH for view-dependent color
    vec3 color = evaluateSH(base, dir);
    color = clamp(color, 0.0, 1.0);

    // Opacity: sigmoid(raw_opacity)
    float rawOpacity = rawData[base + 54];
    float alpha = 1.0 / (1.0 + exp(-rawOpacity));

    // Scale: exp(log_scale) * scaleMod
    vec3 scale = vec3(
        exp(rawData[base + 55]),
        exp(rawData[base + 56]),
        exp(rawData[base + 57])
    ) * scaleMod;

    // Rotation: PLY stores (w,x,y,z), shader expects (x,y,z,w)
    // Normalize quaternion
    vec4 rawRot = vec4(
        rawData[base + 58], // w
        rawData[base + 59], // x
        rawData[base + 60], // y
        rawData[base + 61]  // z
    );
    rawRot = normalize(rawRot);
    vec4 rot = vec4(rawRot.y, rawRot.z, rawRot.w, rawRot.x); // xyzw

    // Write compact rendering splat
    renderSplats[idx].position = vec4(pos, 0.0);
    renderSplats[idx].scale    = vec4(scale, 0.0);
    renderSplats[idx].rotation = rot;
    renderSplats[idx].color    = vec4(color, alpha);
}
)_";

//=============================================================================
// COMPUTE SHADERS FOR DEPTH SORTING
//=============================================================================

/**
 * Compute shader: Generate depth keys from compact rendering splats
 * Transforms view-space Z to a sortable unsigned integer key
 */
static constexpr auto depth_key_shader = R"_(#version 450
layout(local_size_x = 256) in;

struct RenderSplat {
    vec4 position;
    vec4 scale;
    vec4 rotation;
    vec4 color;
};

layout(std430, binding = 0) readonly buffer SplatBuffer {
    RenderSplat splats[];
};

layout(std430, binding = 1) writeonly buffer KeyBuffer {
    uint keys[];
};

layout(std430, binding = 2) writeonly buffer IndexBuffer {
    uint indices[];
};

layout(std140, binding = 3) uniform Params {
    mat4 view;
    uint splatCount;
    float nearPlane;
    float farPlane;
    uint _pad;
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= splatCount) return;

    // Transform to view space
    vec4 viewPos = view * vec4(splats[idx].position.xyz, 1.0);
    float depth = -viewPos.z; // Negate because view space Z is negative

    // Front-to-back sort key: top 16 bits = depth, bottom 16 bits = splat index.
    // The depth gives correct rendering order; the index provides stable
    // tie-breaking for splats at similar depths (same buffer order every frame).
    // This eliminates the "wave" artifact from coherent sort-order swaps.
    // Combined with "under" blending for correct front-to-back compositing.
    const uint keyMax = 0xFFFFFFFFu;
    uint key;
    if (depth <= nearPlane) {
        // Behind camera: draw last, but keep stable index-based sub-order
        key = (0xFFFFu << 16u) | (idx & 0xFFFFu);
    } else {
        float t = log2(depth / nearPlane) / log2(farPlane / nearPlane);
        t = clamp(t, 0.0, 1.0);
        uint depthKey = uint(t * 65535.0);
        key = (depthKey << 16u) | (idx & 0xFFFFu);
    }

    keys[idx] = key;
    indices[idx] = idx;
}
)_";

/**
 * Compute shader: Histogram counting for radix sort
 * Counts occurrences of each digit value
 */
static constexpr auto histogram_shader = R"_(#version 450
layout(local_size_x = 256) in;

layout(std430, binding = 0) readonly buffer KeyBuffer {
    uint keys[];
};

layout(std430, binding = 1) buffer HistogramBuffer {
    uint histogram[]; // 256 buckets * num_workgroups
};

layout(std140, binding = 2) uniform Params {
    uint splatCount;
    uint bitOffset;   // Which 8 bits to sort (0, 8, 16, 24)
    uint numWorkgroups;
    uint _pad;
};

shared uint localHistogram[256];

void main() {
    uint localId = gl_LocalInvocationID.x;
    uint globalId = gl_GlobalInvocationID.x;
    uint workgroupId = gl_WorkGroupID.x;

    // Clear local histogram
    localHistogram[localId] = 0;
    barrier();

    // Count digits in this workgroup
    if (globalId < splatCount) {
        uint key = keys[globalId];
        uint digit = (key >> bitOffset) & 0xFFu;
        atomicAdd(localHistogram[digit], 1);
    }
    barrier();

    // Write local histogram to global memory
    histogram[workgroupId * 256 + localId] = localHistogram[localId];
}
)_";

/**
 * Compute shader: Prefix sum and scatter for radix sort
 * Computes exclusive prefix sum and scatters elements to sorted positions
 */
static constexpr auto sort_scatter_shader = R"_(#version 450
layout(local_size_x = 256) in;

layout(std430, binding = 0) readonly buffer KeyBufferIn {
    uint keysIn[];
};

layout(std430, binding = 1) readonly buffer IndexBufferIn {
    uint indicesIn[];
};

layout(std430, binding = 2) writeonly buffer KeyBufferOut {
    uint keysOut[];
};

layout(std430, binding = 3) writeonly buffer IndexBufferOut {
    uint indicesOut[];
};

layout(std430, binding = 4) buffer HistogramBuffer {
    uint histogram[]; // Global prefix sums
};

layout(std140, binding = 5) uniform Params {
    uint splatCount;
    uint bitOffset;
    uint numWorkgroups;
    uint _pad;
};

shared uint localDigits[256];
shared uint localOffset[256];

void main() {
    uint localId = gl_LocalInvocationID.x;
    uint globalId = gl_GlobalInvocationID.x;
    uint workgroupId = gl_WorkGroupID.x;

    // Load global prefix sum for this workgroup's digit
    localOffset[localId] = histogram[workgroupId * 256 + localId];

    // Load this thread's element
    uint key = 0u;
    uint idx = 0u;
    uint digit = 256u; // invalid sentinel (> any real digit)
    bool valid = globalId < splatCount;
    if (valid) {
        key = keysIn[globalId];
        idx = indicesIn[globalId];
        digit = (key >> bitOffset) & 0xFFu;
    }
    localDigits[localId] = digit;
    barrier();

    if (valid) {
        // Stable rank: count threads with LOWER ID that share the same digit.
        // This is deterministic (no atomicAdd race), so the sort is stable
        // and identical across frames — eliminates flickering.
        uint rank = 0u;
        for (uint i = 0u; i < localId; i++) {
            if (localDigits[i] == digit)
                rank++;
        }

        uint globalPos = localOffset[digit] + rank;
        if (globalPos < splatCount) {
            keysOut[globalPos] = key;
            indicesOut[globalPos] = idx;
        }
    }
}
)_";

/**
 * Compute shader: Global prefix sum on histogram
 * Converts per-workgroup histograms to global exclusive prefix sums.
 *
 * Histogram layout: histogram[workgroup * 256 + digit]
 *
 * The output for each (workgroup, digit) pair must be the global position
 * where that workgroup should start placing elements with that digit.
 * This requires accounting for:
 *   1. All elements with smaller digits (across ALL workgroups)
 *   2. Same-digit elements from earlier workgroups
 *
 * Dispatch: (1, 1, 1) — single workgroup of 256 threads, one per digit.
 */
static constexpr auto prefix_sum_shader = R"_(#version 450
layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer HistogramBuffer {
    uint histogram[]; // Layout: histogram[workgroup * 256 + digit]
};

layout(std140, binding = 1) uniform Params {
    uint numWorkgroups;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

shared uint digitTotal[256];
shared uint digitPrefix[256];

void main() {
    uint digit = gl_LocalInvocationID.x; // 0-255, one thread per digit

    // Step 1: Sum all workgroup counts for this digit
    uint total = 0;
    for (uint wg = 0; wg < numWorkgroups; wg++) {
        total += histogram[wg * 256 + digit];
    }
    digitTotal[digit] = total;
    barrier();

    // Step 2: Thread 0 computes exclusive prefix sum across all digits
    // This determines the global starting offset for each digit bucket
    if (digit == 0) {
        digitPrefix[0] = 0;
        for (uint d = 1; d < 256; d++) {
            digitPrefix[d] = digitPrefix[d-1] + digitTotal[d-1];
        }
    }
    barrier();

    // Step 3: Convert per-workgroup counts to global offsets
    // For each workgroup: offset = digitPrefix[digit] + sum of same-digit counts in earlier workgroups
    uint running = digitPrefix[digit];
    for (uint wg = 0; wg < numWorkgroups; wg++) {
        uint idx = wg * 256 + digit;
        uint val = histogram[idx];
        histogram[idx] = running;
        running += val;
    }
}
)_";

//=============================================================================
// RENDER SHADERS
//=============================================================================

static constexpr auto vertex_shader = R"_(#version 450

// Quad vertex positions
const vec2 positions[6] = vec2[6](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

// Compact rendering splat (output of preprocess compute shader)
struct RenderSplat {
    vec4 position;  // xyz = position
    vec4 scale;     // xyz = scale (already exp'd)
    vec4 rotation;  // quaternion xyzw (already normalized)
    vec4 color;     // RGBA (SH evaluated, sigmoid applied)
};

layout(std430, binding = 0) readonly buffer SplatBuffer {
    RenderSplat splats[];
};

// Sorted indices from depth sort pass
layout(std430, binding = 1) readonly buffer SortedIndices {
    uint sortedIndices[];
};

layout(std140, binding = 2) uniform Uniforms {
    mat4 view;
    mat4 projection;
    mat4 clipSpaceCorr;
    vec2 viewport;
    float _pad0;
    uint useSorting; // 0 = no sorting, 1 = use sorted indices
};

layout(location = 0) out vec2 f_center;  // screen-space splat center (pixels)
layout(location = 1) out vec4 f_color;
layout(location = 2) out vec3 f_conic;

mat3 quatToMat(vec4 q) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    // GLSL mat3 is column-major: mat3(col0, col1, col2)
    return mat3(
        1.0 - 2.0*(y*y + z*z), 2.0*(x*y + w*z), 2.0*(x*z - w*y),   // col 0
        2.0*(x*y - w*z), 1.0 - 2.0*(x*x + z*z), 2.0*(y*z + w*x),   // col 1
        2.0*(x*z + w*y), 2.0*(y*z - w*x), 1.0 - 2.0*(x*x + y*y)    // col 2
    );
}

void main() {
    // Get splat index (sorted or unsorted)
    uint splatIdx = useSorting != 0 ? sortedIndices[gl_InstanceIndex] : gl_InstanceIndex;
    RenderSplat splat = splats[splatIdx];
    vec2 quadPos = positions[gl_VertexIndex];

    // Early opacity cull: skip splats that are nearly invisible
    if (splat.color.a < 1.0 / 255.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    // View space position
    vec4 viewPos = view * vec4(splat.position.xyz, 1.0);

    // Focal lengths in pixels
    float focal = projection[0][0] * viewport.x * 0.5;
    float focal_y = projection[1][1] * viewport.y * 0.5;
    float tanFovX = 0.5 * viewport.x / focal;
    float tanFovY = 0.5 * viewport.y / focal_y;

    // Frustum culling: project to clip space and check NDC bounds
    // (matches INRIA reference: cull behind camera + outside 1.3x viewport)
    vec4 clipPos = projection * viewPos;
    if (clipPos.w <= 0.2) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }
    vec3 ndc = clipPos.xyz / clipPos.w;
    if (abs(ndc.x) > 1.3 || abs(ndc.y) > 1.3) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    // Clamp view-space position to prevent numerical issues at screen edges
    // (matches INRIA CUDA reference: 1.3x FOV tangent)
    float limX = 1.3 * tanFovX;
    float limY = 1.3 * tanFovY;
    float txtz = viewPos.x / viewPos.z;
    float tytz = viewPos.y / viewPos.z;
    viewPos.x = clamp(txtz, -limX, limX) * viewPos.z;
    viewPos.y = clamp(tytz, -limY, limY) * viewPos.z;

    // Build 3D covariance from scale and rotation (already preprocessed)
    // INRIA convention: Sigma = R * S * S^T * R^T = R * S² * R^T
    // The principal axes are the COLUMNS of R.
    vec3 scale = splat.scale.xyz;
    mat3 R = quatToMat(splat.rotation);
    mat3 S = mat3(scale.x, 0, 0, 0, scale.y, 0, 0, 0, scale.z);
    mat3 M = R * S;
    mat3 Sigma = M * transpose(M);

    // 2D covariance via EWA projection
    mat3 W = mat3(view);
    float z2 = viewPos.z * viewPos.z;

    // Jacobian of projection (column-major: mat3(col0, col1, col2))
    mat3 J = mat3(
        focal / viewPos.z, 0.0, 0.0,                                // col 0
        0.0, focal_y / viewPos.z, 0.0,                              // col 1
        -focal * viewPos.x / z2, -focal_y * viewPos.y / z2, 0.0     // col 2
    );

    mat3 T = J * W;
    mat3 cov = T * Sigma * transpose(T);

    float cov_xx = cov[0][0], cov_xy = cov[0][1], cov_yy = cov[1][1];

    // Mip-Splatting 2D filter (Yu et al. 2024): approximate the pixel box filter
    // as a Gaussian and convolve with the projected 2D covariance.
    // Opacity is compensated to preserve each splat's total contribution:
    //   alpha' = alpha * sqrt(det(Sigma) / det(Sigma + kernel_size * I))
    float kernel_size = 0.3;
    float det_0 = max(1e-6, cov_xx * cov_yy - cov_xy * cov_xy);
    cov_xx += kernel_size;
    cov_yy += kernel_size;
    float det_1 = max(1e-6, cov_xx * cov_yy - cov_xy * cov_xy);
    float mipCoef = sqrt(det_0 / det_1);

    float det = cov_xx * cov_yy - cov_xy * cov_xy;
    float mid = 0.5 * (cov_xx + cov_yy);
    float disc = max(0.0, mid * mid - det);
    float lambda1 = mid + sqrt(disc);
    float lambda2 = mid - sqrt(disc);

    // Eigenvectors of 2D covariance for ellipse-aligned quad
    vec2 eigVec1;
    if (abs(cov_xy) > 1e-6) {
        eigVec1 = normalize(vec2(cov_xy, lambda1 - cov_xx));
    } else {
        eigVec1 = (cov_xx >= cov_yy) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    }
    vec2 eigVec2 = vec2(-eigVec1.y, eigVec1.x);

    float maxExtent = 2048.0;
    float r1 = min(ceil(3.0 * sqrt(max(lambda1, 0.0))), maxExtent);
    float r2 = min(ceil(3.0 * sqrt(max(lambda2, 0.0))), maxExtent);

    // Cull degenerate or invisible splats
    if (det < 1e-3 || max(r1, r2) < 0.1) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    // Inverse covariance (conic) for fragment Gaussian evaluation.
    // The cross-term sign must match the screen-space convention of gl_FragCoord:
    //   Vulkan/Metal/D3D (clipSpaceCorr[1][1] < 0): both screen axes flip
    //     relative to J-space, preserving the cross-product sign.
    //   OpenGL (clipSpaceCorr[1][1] > 0): only X flips, requiring correction.
    float inv_det = 1.0 / det;
    float crossSign = sign(clipSpaceCorr[1][1]);
    f_conic = vec3(cov_yy * inv_det, crossSign * cov_xy * inv_det, cov_xx * inv_det);

    // Oriented quad: major axis along eigVec1, minor along eigVec2
    vec2 pixelOffset = quadPos.x * r1 * eigVec1 + quadPos.y * r2 * eigVec2;
    vec2 center = ndc.xy;
    vec2 ndcOffset = pixelOffset * 2.0 / viewport;

    gl_Position = clipSpaceCorr * vec4(center + ndcOffset, ndc.z, 1.0);

    // Score's texture compositing pipeline flips Y when sampling for Vulkan/HLSL/Metal.
    // To match this convention (same as ISF shaders), we undo clipSpaceCorr's Y-flip here
    // so the compositing re-flip produces a correctly oriented final image.
    gl_Position.y = -gl_Position.y;

    // Screen-space center in pixels (matches gl_FragCoord coordinate system)
    vec4 centerClip = clipSpaceCorr * vec4(ndc.xy, ndc.z, 1.0);
    centerClip.y = -centerClip.y;
    f_center = (centerClip.xy / centerClip.w * 0.5 + 0.5) * viewport;

    // Fade out excessively large projected splats.
    float alpha = splat.color.a * mipCoef;
    float maxR = max(r1, r2);
    float fadeRadius = 512.0;
    if (maxR > fadeRadius) {
        float fade = fadeRadius / maxR;
        alpha *= fade;
        if (alpha < 1.0 / 255.0) {
            gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
            return;
        }
    }
    f_color = vec4(splat.color.rgb, alpha);
}
)_";

static constexpr auto fragment_shader = R"_(#version 450

layout(location = 0) in vec2 f_center;  // screen-space splat center (pixels)
layout(location = 1) in vec4 f_color;
layout(location = 2) in vec3 f_conic;

layout(location = 0) out vec4 fragColor;

void main() {
    // Pixel offset from splat center, computed per-fragment for precision.
    // Unlike interpolated UVs, this is exact regardless of quad orientation.
    vec2 d = gl_FragCoord.xy - f_center;

    float power = -0.5 * (f_conic.x * d.x * d.x +
                          2.0 * f_conic.y * d.x * d.y +
                          f_conic.z * d.y * d.y);

    if (power > 0.0) discard;

    float gaussian = exp(power);
    float alpha = min(0.99, gaussian * f_color.a);
    if (alpha < 1.0/255.0) discard;

    fragColor = vec4(f_color.rgb * alpha, alpha);
}
)_";

} // namespace GaussianSplatShaders

} // namespace score::gfx
