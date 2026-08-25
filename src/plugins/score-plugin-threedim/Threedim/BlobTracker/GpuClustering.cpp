#include "GpuClustering.hpp"

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QDebug>
#include <QtGui/private/qrhi_p.h>

#include <cmath>

#include <algorithm>
#include <cstring>
#include <exception>

namespace Threedim::Blobs
{
namespace
{
constexpr int local_size = 256;
constexpr int merge_local_size = 128;

// One scan block covers 1024 histogram entries (256 threads x 4). The table is
// a power of two of at least 1024, so blocks are always full.
constexpr uint32_t scan_block = 1024;

// Grid-stride loops let every pass be clamped to a dispatch size every backend
// accepts, instead of scaling the group count with the point count.
constexpr int max_workgroups = 4096;

[[nodiscard]] int grid_for(int64_t items, int group) noexcept
{
  const int64_t g = (items + group - 1) / group;
  return (int)std::clamp<int64_t>(g, 1, max_workgroups);
}

[[nodiscard]] uint32_t table_size_for(int64_t points) noexcept
{
  uint32_t ts = min_table_size;
  while(ts < (uint32_t)points && ts < max_table_size)
    ts <<= 1;
  return ts;
}

// Prepended to every kernel. The uniform block is declared here because every
// pass binds it at 0, and the three helpers are the pieces the grid's
// correctness argument rests on: cell size equals the cluster distance, so two
// points closer than that always land in the same cell or in touching ones,
// which is what makes the 27-cell neighbourhood exhaustive.
const char* prelude = R"(#version 450

const uint META_WORDS  = 16u;
const uint KNN_SAMPLES = 256u;
const uint BLOB_BASE   = META_WORDS + KNN_SAMPLES;
const uint BLOB_WORDS  = 10u;

layout(std140, binding = 0) uniform Params {
  uint  u_n;
  uint  u_tableSize;
  uint  u_srcStride;
  uint  u_srcOffset;
  float u_invCellSize;
  float u_clusterDist2;
  uint  u_numBlocks;
  uint  u_maxClusters;
  uint  u_minPoints;
  uint  u_maxBlobs;
  uint  u_knnK;
  uint  u_knnSamples;
  uint  u_pad0;
  uint  u_pad1;
  uint  u_pad2;
  uint  u_pad3;
};

bool posValid(vec3 p)
{
  return !any(isnan(p)) && !any(isinf(p)) && all(lessThan(abs(p), vec3(1.0e18)));
}

ivec3 cellOf(vec3 p, float invCellSize)
{
  // Clamped so the float-to-int conversion stays defined for far-flung
  // coordinates. Points that far apart never share a cell anyway.
  return ivec3(floor(clamp(p * invCellSize, vec3(-1.0e9), vec3(1.0e9))));
}

uint bucketOf(ivec3 c, uint tableSize)
{
  return (uint(c.x) * 73856093u ^ uint(c.y) * 19349663u ^ uint(c.z) * 83492791u)
         & (tableSize - 1u);
}
)";

// Positions are read through the source stride/offset, so an interleaved vertex
// buffer is clustered where it already lives.
const char* pos_reader = R"(
layout(std430, binding = 1) readonly buffer PosBuf { uint srcData[]; };

vec3 loadPos(uint i)
{
  uint base = (i * u_srcStride + u_srcOffset) >> 2u;
  return vec3(uintBitsToFloat(srcData[base]),
              uintBitsToFloat(srcData[base + 1u]),
              uintBitsToFloat(srcData[base + 2u]));
}
)";

// ---------------------------------------------------------------------------
// Pass 1 — clear. QRhi has no buffer-fill, so what the original did with
// glClearBufferSubData is a dispatch here. The accumulator is struct-of-arrays
// over maxClusters M precisely so this stays three contiguous ranges:
//   [0, 3M)   centroid sums, cleared to 0
//   [3M, 4M)  point counts, cleared to 0
//   [4M, 7M)  bbox minima as float bits, cleared to +FLT_MAX
//   [7M, 10M) bbox maxima as float bits, cleared to -FLT_MAX
const char* src_reset = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1) buffer CountBuf  { uint cellCount[]; };
layout(std430, binding = 2) buffer AccumBuf  { int  accum[]; };
layout(std430, binding = 3) buffer ResultBuf { int  res[]; };

void main()
{
  const int posMax = floatBitsToInt( 3.402823466e+38);
  const int negMax = floatBitsToInt(-3.402823466e+38);

  uint M      = u_maxClusters;
  uint stride = gl_NumWorkGroups.x * 256u;

  // One flat fill over the whole accumulator, one word per invocation. The
  // struct-of-arrays layout means the fill value is a function of which region
  // the word lands in, which is cheaper than it looks and — unlike a thread
  // clearing all ten of its cluster's slots at 0*M+i .. 9*M+i — keeps the
  // store address a plain induction variable. NVIDIA's GLSL compiler rejects
  // the strided form outright ("lvalue in array access too complex"), so the
  // whole pass silently failed to link and every buffer kept last frame's
  // contents.
  for(uint i = gl_GlobalInvocationID.x; i < 10u * M; i += stride)
  {
    int v = 0;                              // centroid sums and point counts
    if(i >= 7u * M)      v = negMax;        // bbox maxima
    else if(i >= 4u * M) v = posMax;        // bbox minima
    accum[i] = v;
  }

  for(uint i = gl_GlobalInvocationID.x; i < u_tableSize; i += stride)
    cellCount[i] = 0u;

  // -1 marks a sample the k-NN pass did not produce, which is what the host
  // percentile filters on.
  for(uint i = gl_GlobalInvocationID.x; i < KNN_SAMPLES; i += stride)
    res[META_WORDS + i] = floatBitsToInt(-1.0);

  if(gl_GlobalInvocationID.x == 0u)
  {
    // meta[] is ten named fields, not an array range, so they are written as
    // such. The loop this replaces ("for k in 6..META_WORDS: res[k] = 0") is
    // also what NVIDIA's GLSL compiler unrolls into ten SSBO stores it then
    // rejects with "lvalue in array access too complex" — one error per
    // iteration — which took the whole pass down with it.
    res[0]  = posMax;  // bbox min, seeded for the atomic min reduction
    res[1]  = posMax;
    res[2]  = posMax;
    res[3]  = negMax;  // bbox max
    res[4]  = negMax;
    res[5]  = negMax;
    res[6]  = 0;       // quantisation origin, derived by the scan-sums pass
    res[7]  = 0;
    res[8]  = 0;
    res[9]  = 0;       // quantisation scale, likewise
    res[10] = 0;
    res[11] = 0;
    res[12] = 0;       // valid points
    res[13] = 0;       // clusters found
    res[14] = 0;       // blobs emitted
    res[15] = 0;       // cluster overflow flag
  }
}
)";

// ---------------------------------------------------------------------------
// Pass 2 — bounding box reduction fused with the cell histogram: both want one
// pass over the raw positions, so they share it.
const char* src_bbox_count = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 2) buffer CountBuf  { uint cellCount[]; };
layout(std430, binding = 3) buffer ResultBuf { int  res[]; };

shared float sMin[3][256];
shared float sMax[3][256];

void atomicMinF(uint idx, float v)
{
  int e = res[idx];
  for(;;)
  {
    int d = floatBitsToInt(min(v, intBitsToFloat(e)));
    int o = atomicCompSwap(res[idx], e, d);
    if(o == e) return;
    e = o;
  }
}

void atomicMaxF(uint idx, float v)
{
  int e = res[idx];
  for(;;)
  {
    int d = floatBitsToInt(max(v, intBitsToFloat(e)));
    int o = atomicCompSwap(res[idx], e, d);
    if(o == e) return;
    e = o;
  }
}

void main()
{
  uint tid = gl_LocalInvocationID.x;
  vec3 lo = vec3( 3.402823466e+38);
  vec3 hi = vec3(-3.402823466e+38);

  uint stride = gl_NumWorkGroups.x * 256u;
  for(uint i = gl_GlobalInvocationID.x; i < u_n; i += stride)
  {
    vec3 p = loadPos(i);
    if(!posValid(p)) continue;

    lo = min(lo, p);
    hi = max(hi, p);
    atomicAdd(cellCount[bucketOf(cellOf(p, u_invCellSize), u_tableSize)], 1u);
  }

  sMin[0][tid] = lo.x; sMin[1][tid] = lo.y; sMin[2][tid] = lo.z;
  sMax[0][tid] = hi.x; sMax[1][tid] = hi.y; sMax[2][tid] = hi.z;
  barrier();

  for(uint s = 128u; s > 0u; s >>= 1u)
  {
    if(tid < s)
    {
      for(int a = 0; a < 3; a++)
      {
        sMin[a][tid] = min(sMin[a][tid], sMin[a][tid + s]);
        sMax[a][tid] = max(sMax[a][tid], sMax[a][tid + s]);
      }
    }
    barrier();
  }

  if(tid == 0u)
  {
    atomicMinF(0u, sMin[0][0]); atomicMinF(1u, sMin[1][0]); atomicMinF(2u, sMin[2][0]);
    atomicMaxF(3u, sMax[0][0]); atomicMaxF(4u, sMax[1][0]); atomicMaxF(5u, sMax[2][0]);
  }
}
)";

// ---------------------------------------------------------------------------
// Pass 3 — exclusive scan of the histogram, stage 1: scan within each block of
// 1024 and publish the block total.
const char* src_scan_blocks = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1) readonly buffer CountBuf { uint cellCount[]; };
layout(std430, binding = 2)          buffer StartBuf { uint cellStart[]; };
layout(std430, binding = 3)          buffer BlockBuf { uint blockSums[]; };

shared uint sPart[256];

void main()
{
  uint tid  = gl_LocalInvocationID.x;
  uint base = gl_WorkGroupID.x * 1024u + tid * 4u;

  // No bounds guard here, deliberately. The table is a power of two of at least
  // one scan_block and the dispatch is exactly u_tableSize/scan_block groups, so
  // `base` is always in range — while an `if(base >= u_tableSize) return;` would
  // put the barriers below inside non-uniform control flow, which GLSL leaves
  // undefined, for a check that can never fire.
  uvec4 v = uvec4(cellCount[base], cellCount[base + 1u],
                  cellCount[base + 2u], cellCount[base + 3u]);

  uint s0 = 0u;
  uint s1 = v.x;
  uint s2 = s1 + v.y;
  uint s3 = s2 + v.z;
  uint total = s3 + v.w;

  sPart[tid] = total;
  barrier();

  for(uint off = 1u; off < 256u; off <<= 1u)
  {
    uint add = (tid >= off) ? sPart[tid - off] : 0u;
    barrier();
    sPart[tid] += add;
    barrier();
  }

  uint excl = sPart[tid] - total;
  cellStart[base]      = excl + s0;
  cellStart[base + 1u] = excl + s1;
  cellStart[base + 2u] = excl + s2;
  cellStart[base + 3u] = excl + s3;

  if(tid == 255u)
    blockSums[gl_WorkGroupID.x] = sPart[255u];
}
)";

// Stage 2: one workgroup scans the block totals in place. This is also the
// first point where the bounding box and the valid-point count are both final,
// so it derives the fixed-point accumulation frame here rather than paying for
// another dispatch.
const char* src_scan_sums = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1)          buffer StartBuf  { uint cellStart[]; };
layout(std430, binding = 2)          buffer BlockBuf  { uint blockSums[]; };
layout(std430, binding = 3)          buffer ResultBuf { int  res[]; };

shared uint sPart[256];
shared uint sRunning;

void main()
{
  uint tid = gl_LocalInvocationID.x;

  if(tid == 0u) sRunning = 0u;
  barrier();

  for(uint tile = 0u; tile < u_numBlocks; tile += 256u)
  {
    uint idx = tile + tid;
    uint v   = (idx < u_numBlocks) ? blockSums[idx] : 0u;

    sPart[tid] = v;
    barrier();

    for(uint off = 1u; off < 256u; off <<= 1u)
    {
      uint add = (tid >= off) ? sPart[tid - off] : 0u;
      barrier();
      sPart[tid] += add;
      barrier();
    }

    uint run = sRunning;
    if(idx < u_numBlocks)
      blockSums[idx] = run + sPart[tid] - v;
    barrier();

    if(tid == 255u) sRunning = run + sPart[255u];
    barrier();
  }

  if(tid != 0u) return;

  uint total = sRunning;
  cellStart[u_tableSize] = total;
  res[12] = int(total);

  vec3 lo  = vec3(intBitsToFloat(res[0]), intBitsToFloat(res[1]), intBitsToFloat(res[2]));
  vec3 hi  = vec3(intBitsToFloat(res[3]), intBitsToFloat(res[4]), intBitsToFloat(res[5]));
  vec3 ext = max(hi - lo, vec3(0.0));

  // Centroids accumulate as int32 sums of offsets from lo, quantised by scale:
  // GLSL has no atomic float add. The worst case is every valid point joining
  // one cluster, so capping the per-point quantum at 2e9 / count makes overflow
  // impossible. The rounding error is uniform and averages out across the
  // cluster. A FIXED quantisation would overflow at millimetre-scale
  // coordinates, which is why the frame is rebuilt every frame.
  float budget = 2.0e9 / max(float(total), 1.0);
  vec3  scale  = vec3(ext.x > 1.0e-20 ? budget / ext.x : 1.0,
                      ext.y > 1.0e-20 ? budget / ext.y : 1.0,
                      ext.z > 1.0e-20 ? budget / ext.z : 1.0);

  res[6]  = floatBitsToInt(lo.x);
  res[7]  = floatBitsToInt(lo.y);
  res[8]  = floatBitsToInt(lo.z);
  res[9]  = floatBitsToInt(scale.x);
  res[10] = floatBitsToInt(scale.y);
  res[11] = floatBitsToInt(scale.z);
}
)";

// Stage 3: fold each block's offset into its slice of the scan. Zeroing the
// histogram here too turns it into the scatter cursor without a separate clear.
const char* src_scan_add = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1)          buffer CountBuf { uint cellCount[]; };
layout(std430, binding = 2)          buffer StartBuf { uint cellStart[]; };
layout(std430, binding = 3) readonly buffer BlockBuf { uint blockSums[]; };

void main()
{
  uint stride = gl_NumWorkGroups.x * 256u;
  for(uint i = gl_GlobalInvocationID.x; i < u_tableSize; i += stride)
  {
    cellStart[i] += blockSums[i / 1024u];
    cellCount[i] = 0u;
  }
}
)";

// ---------------------------------------------------------------------------
// Pass 6 — reorder points into cell order. Everything downstream reads
// posSorted, so the neighbour walk touches contiguous memory instead of chasing
// a linked list. Union-find roots are seeded here too, saving a dispatch.
const char* src_scatter = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 2)          buffer SortedBuf { float posSorted[]; };
layout(std430, binding = 3)          buffer CursorBuf { uint  cursor[]; };
layout(std430, binding = 4) readonly buffer StartBuf  { uint  cellStart[]; };
layout(std430, binding = 5)          buffer ParentBuf { int   parent[]; };

void main()
{
  uint stride = gl_NumWorkGroups.x * 256u;
  for(uint i = gl_GlobalInvocationID.x; i < u_n; i += stride)
  {
    vec3 p = loadPos(i);
    if(!posValid(p)) continue;

    uint b    = bucketOf(cellOf(p, u_invCellSize), u_tableSize);
    uint slot = cellStart[b] + atomicAdd(cursor[b], 1u);

    posSorted[slot * 3u]      = p.x;
    posSorted[slot * 3u + 1u] = p.y;
    posSorted[slot * 3u + 2u] = p.z;
    parent[slot]              = int(slot);
  }
}
)";

// ---------------------------------------------------------------------------
// Pass 7 — the union-find itself.
const char* src_merge = R"(
layout(local_size_x = 128) in;

layout(std430, binding = 1) readonly buffer SortedBuf { float posSorted[]; };
layout(std430, binding = 2) readonly buffer StartBuf  { uint  cellStart[]; };
layout(std430, binding = 3) coherent buffer ParentBuf { int   parent[]; };
layout(std430, binding = 4) readonly buffer ResultBuf { int   res[]; };

int ufFind(int x)
{
  int r = x;
  while(parent[r] != r)
  {
    int p  = parent[r];
    int gp = parent[p];
    atomicCompSwap(parent[r], p, gp);
    r = gp;
  }
  return r;
}

// Always reparents the higher index onto the lower one, so parent indices
// strictly decrease along any path and ufFind cannot cycle.
void ufUnite(int a, int b)
{
  for(;;)
  {
    a = ufFind(a);
    b = ufFind(b);
    if(a == b) return;
    if(a > b) { int t = a; a = b; b = t; }
    if(atomicCompSwap(parent[b], b, a) == b) return;
  }
}

void main()
{
  int nValid = res[12];

  uint stride = gl_NumWorkGroups.x * 128u;
  for(uint idx = gl_GlobalInvocationID.x; idx < u_n; idx += stride)
  {
    int i = int(idx);
    if(i >= nValid) continue;

    vec3  p = vec3(posSorted[idx * 3u], posSorted[idx * 3u + 1u], posSorted[idx * 3u + 2u]);
    ivec3 c = cellOf(p, u_invCellSize);

    for(int dx = -1; dx <= 1; dx++)
    for(int dy = -1; dy <= 1; dy++)
    for(int dz = -1; dz <= 1; dz++)
    {
      uint b     = bucketOf(c + ivec3(dx, dy, dz), u_tableSize);
      uint first = cellStart[b];
      uint last  = cellStart[b + 1u];

      for(uint j = first; j < last; j++)
      {
        // Each unordered pair is tested once. Cell adjacency is symmetric, so
        // the higher index always sees the lower one.
        if(int(j) <= i) continue;

        vec3 q = vec3(posSorted[j * 3u], posSorted[j * 3u + 1u], posSorted[j * 3u + 2u]);
        vec3 d = p - q;
        if(dot(d, d) < u_clusterDist2)
          ufUnite(i, int(j));
      }
    }
  }
}
)";

// ---------------------------------------------------------------------------
// Pass 8 — collapse every path to its root and hand each root a compact id.
// Only the root itself allocates, so no two invocations can race for one
// cluster's id.
const char* src_label = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1) coherent buffer ParentBuf  { int parent[]; };
layout(std430, binding = 2)          buffer ClusterBuf { int clusterId[]; };
layout(std430, binding = 3) coherent buffer ResultBuf  { int res[]; };

void main()
{
  int nValid = res[12];

  uint stride = gl_NumWorkGroups.x * 256u;
  for(uint idx = gl_GlobalInvocationID.x; idx < u_n; idx += stride)
  {
    int i = int(idx);
    if(i >= nValid) continue;

    int r = i;
    while(parent[r] != r)
    {
      int p  = parent[r];
      int gp = parent[p];
      parent[r] = gp;
      r = gp;
    }
    parent[i] = r;

    if(r != i) continue;

    int id = atomicAdd(res[13], 1);
    if(id < int(u_maxClusters))
    {
      clusterId[i] = id;
    }
    else
    {
      clusterId[i] = -1;
      res[15] = 1;
    }
  }
}
)";

const char* src_propagate = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1) readonly buffer ParentBuf  { int parent[]; };
layout(std430, binding = 2)          buffer ClusterBuf { int clusterId[]; };
layout(std430, binding = 3) readonly buffer ResultBuf  { int res[]; };

void main()
{
  int nValid = res[12];

  uint stride = gl_NumWorkGroups.x * 256u;
  for(uint idx = gl_GlobalInvocationID.x; idx < u_n; idx += stride)
  {
    int i = int(idx);
    if(i >= nValid) continue;
    clusterId[i] = clusterId[parent[i]];
  }
}
)";

const char* src_accumulate = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1) readonly buffer SortedBuf  { float posSorted[]; };
layout(std430, binding = 2) readonly buffer ClusterBuf { int   clusterId[]; };
layout(std430, binding = 3) coherent buffer AccumBuf   { int   accum[]; };
layout(std430, binding = 4) readonly buffer ResultBuf  { int   res[]; };

void atomicMinF(uint idx, float v)
{
  int e = accum[idx];
  for(;;)
  {
    int d = floatBitsToInt(min(v, intBitsToFloat(e)));
    int o = atomicCompSwap(accum[idx], e, d);
    if(o == e) return;
    e = o;
  }
}

void atomicMaxF(uint idx, float v)
{
  int e = accum[idx];
  for(;;)
  {
    int d = floatBitsToInt(max(v, intBitsToFloat(e)));
    int o = atomicCompSwap(accum[idx], e, d);
    if(o == e) return;
    e = o;
  }
}

void main()
{
  int  nValid = res[12];
  uint M      = u_maxClusters;

  vec3 lo    = vec3(intBitsToFloat(res[6]),  intBitsToFloat(res[7]),  intBitsToFloat(res[8]));
  vec3 scale = vec3(intBitsToFloat(res[9]),  intBitsToFloat(res[10]), intBitsToFloat(res[11]));

  uint stride = gl_NumWorkGroups.x * 256u;
  for(uint i = gl_GlobalInvocationID.x; i < u_n; i += stride)
  {
    if(int(i) >= nValid) continue;

    int cid = clusterId[i];
    if(cid < 0) continue;

    vec3  p = vec3(posSorted[i * 3u], posSorted[i * 3u + 1u], posSorted[i * 3u + 2u]);
    ivec3 q = ivec3(clamp((p - lo) * scale, vec3(0.0), vec3(2.1e9)));
    uint  c = uint(cid);

    atomicAdd(accum[c],          q.x);
    atomicAdd(accum[M + c],      q.y);
    atomicAdd(accum[2u * M + c], q.z);
    atomicAdd(accum[3u * M + c], 1);

    atomicMinF(4u * M + c, p.x);
    atomicMinF(5u * M + c, p.y);
    atomicMinF(6u * M + c, p.z);
    atomicMaxF(7u * M + c, p.x);
    atomicMaxF(8u * M + c, p.y);
    atomicMaxF(9u * M + c, p.z);
  }
}
)";

const char* src_build_blobs = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1) readonly buffer AccumBuf  { int accum[]; };
layout(std430, binding = 2) coherent buffer ResultBuf { int res[]; };

void main()
{
  uint M = u_maxClusters;

  vec3 lo    = vec3(intBitsToFloat(res[6]),  intBitsToFloat(res[7]),  intBitsToFloat(res[8]));
  vec3 scale = vec3(intBitsToFloat(res[9]),  intBitsToFloat(res[10]), intBitsToFloat(res[11]));

  uint stride = gl_NumWorkGroups.x * 256u;
  for(uint i = gl_GlobalInvocationID.x; i < M; i += stride)
  {
    int count = accum[3u * M + i];
    if(count < int(u_minPoints)) continue;

    int idx = atomicAdd(res[14], 1);
    if(idx >= int(u_maxBlobs)) continue;

    vec3 sums = vec3(float(accum[i]), float(accum[M + i]), float(accum[2u * M + i]));
    vec3 c    = lo + (sums / float(count)) / scale;

    uint o = BLOB_BASE + uint(idx) * BLOB_WORDS;
    res[o + 0u] = floatBitsToInt(c.x);
    res[o + 1u] = floatBitsToInt(c.y);
    res[o + 2u] = floatBitsToInt(c.z);
    res[o + 3u] = accum[4u * M + i];
    res[o + 4u] = accum[5u * M + i];
    res[o + 5u] = accum[6u * M + i];
    res[o + 6u] = accum[7u * M + i];
    res[o + 7u] = accum[8u * M + i];
    res[o + 8u] = accum[9u * M + i];
    res[o + 9u] = count;
  }
}
)";

// ---------------------------------------------------------------------------
// Pass 12 — distance to the k-th nearest neighbour for a subsample, which the
// host turns into a cluster distance by taking a percentile.
//
// Candidates come from the sample's own 27-cell neighbourhood in the grid the
// pipeline has already built, so every neighbour that could be among the
// nearest k is actually examined. Subsampling the candidates instead would
// inflate the measured spacing by a factor that depends on how many points were
// skipped, and silently rescale auto-scale with the size of the cloud.
const char* src_knn = R"(
layout(local_size_x = 256) in;

layout(std430, binding = 1) readonly buffer SortedBuf { float posSorted[]; };
layout(std430, binding = 2) readonly buffer StartBuf  { uint  cellStart[]; };
layout(std430, binding = 3)          buffer ResultBuf { int   res[]; };

// Only reached while the cluster distance is far too large for the cloud, which
// lasts a frame or two before the estimate settles.
const int MAX_CANDIDATES = 2048;

void main()
{
  uint tid = gl_GlobalInvocationID.x;
  if(tid >= u_knnSamples) return;

  int nValid = res[12];
  if(nValid <= int(u_knnK)) return;

  // Points are already in cell order, so striding the sorted index is a
  // spatially even sample of the cloud.
  uint idx = tid * uint(nValid) / u_knnSamples;
  if(idx >= uint(nValid)) idx = uint(nValid) - 1u;

  vec3  p = vec3(posSorted[idx * 3u], posSorted[idx * 3u + 1u], posSorted[idx * 3u + 2u]);
  ivec3 c = cellOf(p, u_invCellSize);

  float topK[8];
  for(int t = 0; t < 8; t++) topK[t] = 3.402823466e+38;

  int k       = int(u_knnK);
  int found   = 0;
  int scanned = 0;

  for(int n = 0; n < 27 && scanned < MAX_CANDIDATES; n++)
  {
    // Visit the sample's own cell first, so the nearest neighbours are seen
    // before the candidate cap can bite.
    int m = (n == 0) ? 13 : ((n <= 13) ? n - 1 : n);
    ivec3 d = ivec3(m % 3 - 1, (m / 3) % 3 - 1, m / 9 - 1);

    uint b     = bucketOf(c + d, u_tableSize);
    uint first = cellStart[b];
    uint last  = cellStart[b + 1u];

    for(uint j = first; j < last && scanned < MAX_CANDIDATES; j++)
    {
      if(j == idx) continue;
      scanned++;
      found++;

      vec3  q  = vec3(posSorted[j * 3u], posSorted[j * 3u + 1u], posSorted[j * 3u + 2u]);
      vec3  e  = q - p;
      float d2 = dot(e, e);
      if(d2 >= topK[k - 1]) continue;

      topK[k - 1] = d2;
      for(int t = k - 2; t >= 0; t--)
      {
        if(topK[t + 1] >= topK[t]) break;
        float tmp = topK[t]; topK[t] = topK[t + 1]; topK[t + 1] = tmp;
      }
    }
  }

  // Fewer than k neighbours within reach means the cell is smaller than the
  // real spacing. Reporting the cell size grows the estimate, so the next frame
  // searches wider and the loop converges upward instead of stalling.
  float r = (found >= k) ? sqrt(topK[k - 1]) : (1.0 / u_invCellSize);
  res[META_WORDS + tid] = floatBitsToInt(r);
}
)";

const char* pass_name(int p)
{
  static const char* names[]
      = {"reset", "bbox+count", "scan-blocks", "scan-sums",  "scan-add",    "scatter",
         "merge", "label",      "propagate",   "accumulate", "build-blobs", "knn"};
  return names[p];
}

// The uniform block, mirroring `Params` in the prelude.
struct alignas(16) GpuParams
{
  uint32_t n;
  uint32_t table_size;
  uint32_t src_stride;
  uint32_t src_offset;
  float inv_cell_size;
  float cluster_dist2;
  uint32_t num_blocks;
  uint32_t max_clusters;
  uint32_t min_points;
  uint32_t max_blobs;
  uint32_t knn_k;
  uint32_t knn_samples;
  uint32_t pad[4];
};
static_assert(sizeof(GpuParams) == 64);

QRhiBuffer* make_storage(QRhi& rhi, int64_t bytes, const char* name)
{
  auto* b = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, bytes);
  b->setName(name);
  if(!b->create())
  {
    delete b;
    return nullptr;
  }
  return b;
}
}

GpuClusterPipeline::~GpuClusterPipeline()
{
  release();
}

bool GpuClusterPipeline::buildKernel(
    const score::gfx::RenderState& state, QRhi& rhi, Pass p, const char* body)
{
  QString code = QString::fromUtf8(prelude);
  // Every pass that touches the raw cloud needs the strided reader; the others
  // read posSorted and must not declare binding 1 as the source buffer.
  if(p == BboxCount || p == Scatter)
    code += QString::fromUtf8(pos_reader);
  code += QString::fromUtf8(body);

  // makeCompute throws on a compile failure; a bad shader must degrade to "this
  // node does nothing", not tear down the render thread.
  QShader shader;
  try
  {
    shader = score::gfx::makeCompute(state, code);
  }
  catch(const std::exception& e)
  {
    qWarning() << "BlobClustering: compute shader" << pass_name(p)
               << "failed to compile:" << e.what();
    return false;
  }
  catch(...)
  {
    qWarning() << "BlobClustering: compute shader" << pass_name(p)
               << "failed to compile";
    return false;
  }

  if(!shader.isValid())
  {
    qWarning() << "BlobClustering: compute shader is not valid:" << pass_name(p);
    return false;
  }

  auto& k = m_kernels[p];
  k.srb = rhi.newShaderResourceBindings();
  k.pipeline = rhi.newComputePipeline();
  k.pipeline->setShaderStage({QRhiShaderStage::Compute, shader});
  k.pipeline->setShaderResourceBindings(k.srb);
  return true;
}

bool GpuClusterPipeline::init(
    const score::gfx::RenderState& state, QRhi& rhi, int64_t max_points, int max_blobs)
{
  release();

  if(!rhi.isFeatureSupported(QRhi::Compute))
  {
    qWarning() << "BlobClustering: this backend has no compute support";
    return false;
  }

  m_ubo = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256);
  m_ubo->setName("BlobClustering::params");
  if(!m_ubo->create())
  {
    release();
    return false;
  }

  static const struct
  {
    Pass p;
    const char* src;
  } kernels[] = {
      {Reset, src_reset},
      {BboxCount, src_bbox_count},
      {ScanBlocks, src_scan_blocks},
      {ScanSums, src_scan_sums},
      {ScanAdd, src_scan_add},
      {Scatter, src_scatter},
      {Merge, src_merge},
      {Label, src_label},
      {Propagate, src_propagate},
      {Accumulate, src_accumulate},
      {BuildBlobs, src_build_blobs},
      {Knn, src_knn},
  };

  for(const auto& [p, src] : kernels)
  {
    if(!buildKernel(state, rhi, p, src))
    {
      release();
      return false;
    }
  }

  if(!allocate(rhi, std::max<int64_t>(max_points, 1), std::max(max_blobs, 1)))
  {
    release();
    return false;
  }

  m_ready = true;
  return true;
}

bool GpuClusterPipeline::allocate(QRhi& rhi, int64_t points, int max_blobs)
{
  destroyBuffers(/* deferred */ true);

  m_capacity = points;
  m_max_blobs = std::clamp(max_blobs, 1, max_blobs_limit);
  m_table_size = table_size_for(points);

  const int64_t blocks = std::max<int64_t>(m_table_size / scan_block, 1);
  const int64_t M = max_clusters;

  m_sorted = make_storage(rhi, points * 3 * sizeof(float), "BlobClustering::sorted");
  m_count = make_storage(rhi, (int64_t)m_table_size * 4, "BlobClustering::cellCount");
  m_start
      = make_storage(rhi, ((int64_t)m_table_size + 1) * 4, "BlobClustering::cellStart");
  m_blocks = make_storage(rhi, blocks * 4, "BlobClustering::blockSums");
  m_parent = make_storage(rhi, points * 4, "BlobClustering::parent");
  m_cluster = make_storage(rhi, points * 4, "BlobClustering::clusterId");
  m_accum = make_storage(rhi, M * 10 * 4, "BlobClustering::accum");
  m_results
      = make_storage(rhi, gpu_results_bytes(m_max_blobs), "BlobClustering::results");

  if(!m_sorted || !m_count || !m_start || !m_blocks || !m_parent || !m_cluster
     || !m_accum || !m_results)
  {
    qWarning() << "BlobClustering: scratch buffer allocation failed";
    destroyBuffers(/* deferred */ true);
    return false;
  }

  m_bindings_dirty = true;
  return true;
}

bool GpuClusterPipeline::bindAll(QRhi& rhi)
{
  using B = QRhiShaderResourceBinding;
  constexpr auto cs = B::ComputeStage;

  const auto ubo = B::uniformBuffer(0, cs, m_ubo);

  // Each pass gets exactly the bindings its shader declares, in the same order:
  // an SRB whose layout does not match the pipeline is either flagged by the
  // validation layer or silently reads the wrong buffer.
  m_kernels[Reset].srb->setBindings(
      {ubo, B::bufferLoadStore(1, cs, m_count), B::bufferLoadStore(2, cs, m_accum),
       B::bufferLoadStore(3, cs, m_results)});

  m_kernels[BboxCount].srb->setBindings(
      {ubo, B::bufferLoad(1, cs, m_src.buffer), B::bufferLoadStore(2, cs, m_count),
       B::bufferLoadStore(3, cs, m_results)});

  m_kernels[ScanBlocks].srb->setBindings(
      {ubo, B::bufferLoad(1, cs, m_count), B::bufferLoadStore(2, cs, m_start),
       B::bufferLoadStore(3, cs, m_blocks)});

  m_kernels[ScanSums].srb->setBindings(
      {ubo, B::bufferLoadStore(1, cs, m_start), B::bufferLoadStore(2, cs, m_blocks),
       B::bufferLoadStore(3, cs, m_results)});

  m_kernels[ScanAdd].srb->setBindings(
      {ubo, B::bufferLoadStore(1, cs, m_count), B::bufferLoadStore(2, cs, m_start),
       B::bufferLoad(3, cs, m_blocks)});

  m_kernels[Scatter].srb->setBindings(
      {ubo, B::bufferLoad(1, cs, m_src.buffer), B::bufferLoadStore(2, cs, m_sorted),
       B::bufferLoadStore(3, cs, m_count), B::bufferLoad(4, cs, m_start),
       B::bufferLoadStore(5, cs, m_parent)});

  m_kernels[Merge].srb->setBindings(
      {ubo, B::bufferLoad(1, cs, m_sorted), B::bufferLoad(2, cs, m_start),
       B::bufferLoadStore(3, cs, m_parent), B::bufferLoad(4, cs, m_results)});

  m_kernels[Label].srb->setBindings(
      {ubo, B::bufferLoadStore(1, cs, m_parent), B::bufferLoadStore(2, cs, m_cluster),
       B::bufferLoadStore(3, cs, m_results)});

  m_kernels[Propagate].srb->setBindings(
      {ubo, B::bufferLoad(1, cs, m_parent), B::bufferLoadStore(2, cs, m_cluster),
       B::bufferLoad(3, cs, m_results)});

  m_kernels[Accumulate].srb->setBindings(
      {ubo, B::bufferLoad(1, cs, m_sorted), B::bufferLoad(2, cs, m_cluster),
       B::bufferLoadStore(3, cs, m_accum), B::bufferLoad(4, cs, m_results)});

  m_kernels[BuildBlobs].srb->setBindings(
      {ubo, B::bufferLoad(1, cs, m_accum), B::bufferLoadStore(2, cs, m_results)});

  m_kernels[Knn].srb->setBindings(
      {ubo, B::bufferLoad(1, cs, m_sorted), B::bufferLoad(2, cs, m_start),
       B::bufferLoadStore(3, cs, m_results)});

  bool ok = true;
  for(int i = 0; i < PassCount; i++)
  {
    auto& k = m_kernels[i];
    // Rebuilding the SRB is fine (and is what the other Threedim compute
    // strategies do); the PIPELINE must only ever be created once. Every rebind
    // here keeps the same binding indices and types, so the layout stays
    // compatible with the pipeline built against the first one.
    k.srb->create();
    if(!m_pipelines_created && !k.pipeline->create())
    {
      qWarning() << "BlobClustering: compute pipeline creation failed for pass"
                 << pass_name(i);
      ok = false;
    }
  }
  m_pipelines_created = true;
  m_bindings_dirty = false;
  return ok;
}

bool GpuClusterPipeline::update(
    const score::gfx::RenderState& state, QRhi& rhi, const GpuPointSource& src,
    const ClusterParams& params)
{
  if(!m_ready || !src.valid())
    return false;

  const int wanted_blobs = std::clamp(params.max_blobs, 1, max_blobs_limit);
  const bool grew = src.count > m_capacity || wanted_blobs != m_max_blobs;
  const bool buffer_changed = src.buffer != m_src.buffer;

  m_src = src;
  m_params = params;

  if(grew)
  {
    if(!allocate(rhi, src.count, wanted_blobs))
    {
      m_ready = false;
      return false;
    }
  }
  else if(buffer_changed)
  {
    m_bindings_dirty = true;
  }

  if(m_bindings_dirty && !bindAll(rhi))
  {
    m_ready = false;
    return false;
  }

  return true;
}

void GpuClusterPipeline::runCompute(
    QRhi& rhi, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res)
{
  if(!m_ready || !m_src.valid() || !m_results)
    return;

  const float cluster_dist = std::max(m_params.cluster_dist, 1e-8f);
  const int64_t n = m_src.count;

  GpuParams p{};
  p.n = (uint32_t)n;
  p.table_size = m_table_size;
  p.src_stride = (uint32_t)m_src.stride_bytes;
  p.src_offset = (uint32_t)m_src.offset_bytes;
  p.inv_cell_size = 1.f / cluster_dist;
  p.cluster_dist2 = cluster_dist * cluster_dist;
  p.num_blocks = std::max<uint32_t>(m_table_size / scan_block, 1);
  p.max_clusters = max_clusters;
  p.min_points = (uint32_t)std::max(m_params.min_points, 1);
  p.max_blobs = (uint32_t)m_max_blobs;
  p.knn_k = (uint32_t)std::clamp(m_params.knn_k, 0, knn_max_k);
  p.knn_samples = (uint32_t)std::min<int64_t>(n, knn_samples);

  res->updateDynamicBuffer(m_ubo, 0, sizeof(p), &p);

  const int grid_points = grid_for(n, local_size);
  const int grid_merge = grid_for(n, merge_local_size);
  const int grid_table = grid_for(m_table_size, local_size);
  // One invocation per accumulator word (10 regions of max_clusters), which is
  // the largest of the three ranges the clear covers. The loops are grid-strided,
  // so the max_workgroups clamp only costs iterations.
  const int grid_clear
      = grid_for(std::max<int64_t>(m_table_size, 10ll * max_clusters), local_size);
  const int grid_cluster = grid_for(max_clusters, local_size);

  // Every pass consumes the whole of its predecessor's output, so each boundary
  // is a real barrier. They are separate compute passes rather than one pass
  // with barriers between dispatches because QRhi only exposes the backend
  // barrier through beginExternal/endExternal.
  const auto dispatch = [&](Pass pass, int groups) {
    cb.beginComputePass(res, QRhiCommandBuffer::BeginPassFlag::ExternalContent);
    res = nullptr;

    cb.setComputePipeline(m_kernels[pass].pipeline);
    cb.setShaderResources(m_kernels[pass].srb);
    cb.dispatch(groups, 1, 1);

    cb.beginExternal();
    score::gfx::insertComputeBarrier(rhi, cb);
    cb.endExternal();
    cb.endComputePass();

    res = rhi.nextResourceUpdateBatch();
  };

  dispatch(Reset, grid_clear);
  dispatch(BboxCount, grid_points);
  // The only pass without a grid-stride loop: one group per scan_block entries,
  // exactly. max_table_size / scan_block is 4096, which is why max_workgroups is
  // 4096 — the clamp can never bite here, and a smaller one would silently
  // leave the tail of the histogram unscanned.
  dispatch(
      ScanBlocks,
      (int)std::clamp<int64_t>(m_table_size / scan_block, 1, max_workgroups));
  dispatch(ScanSums, 1);
  dispatch(ScanAdd, grid_table);
  dispatch(Scatter, grid_points);
  dispatch(Merge, grid_merge);
  dispatch(Label, grid_points);
  dispatch(Propagate, grid_points);
  dispatch(Accumulate, grid_points);
  dispatch(BuildBlobs, grid_cluster);

  if(p.knn_k > 0)
    dispatch(Knn, grid_for(p.knn_samples, local_size));
}

void GpuClusterPipeline::destroyBuffers(bool deferred) noexcept
{
  for(auto** b :
      {&m_sorted, &m_count, &m_start, &m_blocks, &m_parent, &m_cluster, &m_accum,
       &m_results})
  {
    if(*b)
    {
      // A reallocation happens mid-session, with the SRBs still referencing
      // these and a frame possibly in flight, so the old buffers go through
      // QRhi's own deletion queue. Teardown is the one case where the caller
      // guarantees no frame is live and immediate deletion is correct.
      if(deferred)
        (*b)->deleteLater();
      else
        delete *b;
    }
    *b = nullptr;
  }
  m_capacity = 0;
  m_max_blobs = 0;
}

void GpuClusterPipeline::release() noexcept
{
  bool ok = true;
  for(int i = 0; i < PassCount; i++)
  {
    auto& k = m_kernels[i];
    delete k.pipeline;
    k.pipeline = nullptr;
    delete k.srb;
    k.srb = nullptr;
  }

  delete m_ubo;
  m_ubo = nullptr;

  destroyBuffers(/* deferred */ false);

  m_src = {};
  m_table_size = 0;
  m_ready = false;
  m_pipelines_created = false;
  m_bindings_dirty = true;
}

int decode_gpu_results(
    const void* data, int64_t bytes, int max_blobs, std::vector<Detection>& blobs,
    ClusterResult& result)
{
  result = ClusterResult{};
  blobs.clear();

  max_blobs = std::clamp(max_blobs, 1, max_blobs_limit);
  if(!data || bytes < (int64_t)(gpu_meta_words * sizeof(uint32_t)))
    return 0;

  const auto* meta = reinterpret_cast<const int32_t*>(data);

  const auto* metaf = reinterpret_cast<const float*>(data);
  for(int a = 0; a < 3; a++)
  {
    result.bounds_min[a] = metaf[a];
    result.bounds_max[a] = metaf[3 + a];
  }

  result.num_valid_points = std::max(meta[12], 0);
  result.num_clusters = std::max(meta[13], 0);
  result.cluster_overflow = meta[15] != 0;

  const int blob_count = std::clamp(meta[14], 0, max_blobs);
  const int64_t needed
      = (int64_t)(gpu_meta_words + knn_samples + blob_count * gpu_blob_words)
        * sizeof(uint32_t);
  if(bytes < needed)
    return 0;

  const auto* words = reinterpret_cast<const float*>(data);

  blobs.reserve(blob_count);
  for(int i = 0; i < blob_count; i++)
  {
    const float* b = words + gpu_meta_words + knn_samples + i * gpu_blob_words;
    Detection d;
    d.cx = b[0];
    d.cy = b[1];
    d.cz = b[2];
    d.bmnx = b[3];
    d.bmny = b[4];
    d.bmnz = b[5];
    d.bmxx = b[6];
    d.bmxy = b[7];
    d.bmxz = b[8];
    d.point_count = reinterpret_cast<const int32_t*>(b)[9];
    blobs.push_back(d);
  }
  result.num_blobs = (int)blobs.size();

  // The k-NN percentile is host side, as upstream: the samples are already in
  // the readback, so it costs nothing and keeps the pipeline to one sync.
  const int samples = std::min<int>(knn_samples, result.num_valid_points);
  float valid[knn_samples];
  int count = 0;
  for(int i = 0; i < samples; i++)
  {
    const float v = words[gpu_meta_words + i];
    if(v > 0.f && v < 1e18f)
      valid[count++] = v;
  }

  if(count > 0)
  {
    // The 75th percentile rather than the median: more forgiving of sparse
    // regions, so thin parts of a body still join their cluster.
    const int q = std::min((count * 3) / 4, count - 1);
    std::nth_element(valid, valid + q, valid + count);
    result.knn_spacing = valid[q];
  }

  return result.num_blobs;
}

}
