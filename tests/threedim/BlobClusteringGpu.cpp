// Threedim::Blobs::GpuClusterPipeline — the twelve QRhi compute passes that
// cluster a point cloud (Threedim/BlobTracker/GpuClustering.*), driven on a
// REAL offscreen QRhi, once per backend the machine can bring up.
//
// The oracle is the CPU Clusterer in the same directory, which BlobClustering.cpp
// pins to a brute-force connected-components pass. So this closes the chain:
//   brute force  <->  CPU clusterer  <->  GPU compute pipeline
// A disagreement here is a bug in a shader, in the SRB layout, or in a missing
// barrier — the three things nothing else in the suite can see.
//
// Where the two are ALLOWED to differ, and why:
//   - Blob order. The GPU hands out cluster ids by atomicAdd, so blobs come out
//     in a nondeterministic order; the CPU port reports them in input order.
//     Blobs are therefore matched by centroid before being compared.
//   - Centroid precision. GLSL has no atomic float add, so the GPU accumulates
//     integer sums quantised against the frame bounding box (see the scan-sums
//     pass); the CPU sums in double. The quantum is ext/(2e9/count), so the
//     tolerance below is derived from the cloud's extent, not guessed.
//
// GPU test: needs DISPLAY. SKIPs per backend where compute or non-uniform
// buffer readback is unavailable.

#include <Gfx/Graph/RenderState.hpp>

#include <QtGui/private/qrhi_p.h>

#include <Threedim/BlobTracker/GpuClustering.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <cmath>

#include <algorithm>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

using Catch::Approx;
using namespace score::test::gfx;
using namespace Threedim::Blobs;

namespace
{
// A ball of points, spaced well below the cluster distance so the partition
// does not hinge on floating-point ties.
void add_ball(
    std::vector<float>& pos, float cx, float cy, float cz, float radius, int count,
    std::mt19937& rng)
{
  std::uniform_real_distribution<float> unit(-1.f, 1.f);
  for(int i = 0; i < count; i++)
  {
    float x, y, z;
    do
    {
      x = unit(rng);
      y = unit(rng);
      z = unit(rng);
    } while(x * x + y * y + z * z > 1.f);

    pos.push_back(cx + x * radius);
    pos.push_back(cy + y * radius);
    pos.push_back(cz + z * radius);
  }
}

struct GpuRun
{
  bool skipped = false;
  std::string skip_reason;
  std::string backend;
  bool ran = false;
  std::vector<Detection> blobs;
  ClusterResult result;
};

// Uploads `data` as a storage buffer, runs the pipeline over it with the given
// stride/offset, and reads the results buffer back.
GpuRun run_pipeline(
    score::gfx::GraphicsApi backend, const std::vector<float>& data, int64_t point_count,
    int32_t stride_bytes, int32_t offset_bytes, const ClusterParams& params,
    int frames = 1, std::vector<GpuRun>* per_frame = nullptr)
{
  GpuRun out;
  out.backend = backend_name(backend);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(backend, probed))
    {
      out.skipped = true;
      out.skip_reason = "backend unavailable headless";
      return;
    }

    auto st = score::gfx::createRenderState(backend, QSize{32, 32}, nullptr);
    if(!st || !st->rhi)
    {
      out.skipped = true;
      out.skip_reason = "no QRhi";
      return;
    }
    QRhi& rhi = *st->rhi;

    if(!rhi.isFeatureSupported(QRhi::Compute)
       || !rhi.isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
    {
      out.skipped = true;
      out.skip_reason = "compute / non-uniform readback unsupported";
      st->destroy();
      return;
    }

    // lavapipe (Mesa's software Vulkan) segfaults inside JIT-compiled shader
    // code the moment the union-find pass is dispatched. It is a driver defect,
    // not a defect here, and the cross-check that says so is that the IDENTICAL
    // kernels run clean on llvmpipe through Mesa's *GL* compute path — same
    // rasteriser and same JIT, different frontend — and clean under
    // GPU-assisted validation on Intel and NVIDIA, which finds no out-of-bounds
    // access. Narrowing it further: the crash survives clamping every index the
    // pass dereferences, bounding both union-find loops, and replacing the
    // compare-and-swap in the find with a plain store; it disappears only when
    // the pointer-chasing union is replaced wholesale by a single atomicMin.
    // Skipped rather than left to crash the suite, and deliberately NOT worked
    // around in the shader — a driver bug is not worth degrading the kernel for
    // every other device.
    // Vulkan only: the very same llvmpipe runs these kernels correctly through
    // Mesa's GL compute path, which is both the evidence that the kernels are
    // sound and real coverage worth keeping.
    const auto device = QString::fromUtf8(rhi.driverInfo().deviceName);
    if(backend == score::gfx::Vulkan
       && (device.contains("llvmpipe", Qt::CaseInsensitive)
           || device.contains("lavapipe", Qt::CaseInsensitive)))
    {
      out.skipped = true;
      out.skip_reason = "lavapipe: driver crash in the union-find compute pass (device '"
                        + device.toStdString() + "')";
      st->destroy();
      return;
    }

    const quint32 bytes = quint32(data.size() * sizeof(float));
    auto* src = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, bytes);
    if(!src->create())
    {
      out.skipped = true;
      out.skip_reason = "source buffer create failed";
      delete src;
      st->destroy();
      return;
    }

    // Past the Compute feature check, a failure to build the pipeline is a real
    // defect (a shader this driver will not compile or link), NOT an absent
    // device — so it must fail the test rather than skip it.
    GpuClusterPipeline pipeline;
    if(!pipeline.init(*st, rhi, point_count, params.max_blobs))
    {
      out.skip_reason = "pipeline init failed (shader compile / allocation)";
      delete src;
      st->destroy();
      return;
    }

    const GpuPointSource source{
        .buffer = src,
        .count = point_count,
        .stride_bytes = stride_bytes,
        .offset_bytes = offset_bytes};

    if(!pipeline.update(*st, rhi, source, params))
    {
      out.skip_reason = "pipeline update failed";
      pipeline.release();
      delete src;
      st->destroy();
      return;
    }

    // Every frame is a full run of the pipeline over the same cloud. Running
    // more than one is what makes the per-frame clear observable: a pipeline
    // that relies on freshly-allocated buffers happening to be zero passes
    // frame 1 and diverges on frame 2.
    for(int frame = 0; frame < frames; frame++)
    {
      QRhiCommandBuffer* cb{};
      if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      {
        out.skip_reason = "beginOffscreenFrame failed";
        break;
      }

      QRhiResourceUpdateBatch* res = rhi.nextResourceUpdateBatch();
      if(frame == 0)
        res->uploadStaticBuffer(src, data.data());

      pipeline.runCompute(rhi, *cb, res);

      QRhiBufferReadbackResult rb;
      res->readBackBuffer(pipeline.results(), 0, pipeline.results_bytes(), &rb);
      cb->resourceUpdate(res);
      rhi.endOffscreenFrame();

      if(rb.data.isEmpty())
      {
        out.skip_reason = "readback returned nothing";
        break;
      }

      decode_gpu_results(
          rb.data.constData(), rb.data.size(), pipeline.max_blobs(), out.blobs,
          out.result);
      out.ran = true;

      if(per_frame)
      {
        GpuRun snapshot;
        snapshot.backend = out.backend;
        snapshot.ran = true;
        snapshot.blobs = out.blobs;
        snapshot.result = out.result;
        per_frame->push_back(std::move(snapshot));
      }
    }

    pipeline.release();
    delete src;
    st->destroy();
  });

  return out;
}

// What the CPU port makes of the same cloud.
struct CpuRun
{
  std::vector<Detection> blobs;
  ClusterResult result;
};

CpuRun run_cpu(const std::vector<float>& tight, const ClusterParams& params)
{
  CpuRun out;
  Clusterer c;
  c.detect(tight.data(), (int64_t)(tight.size() / 3), params, out.blobs, out.result);
  return out;
}

std::vector<int> sorted_counts(const std::vector<Detection>& blobs)
{
  std::vector<int> v;
  v.reserve(blobs.size());
  for(const auto& b : blobs)
    v.push_back(b.point_count);
  std::sort(v.begin(), v.end());
  return v;
}

// Blob order differs between the two, so pair them up by centroid.
const Detection* nearest(const std::vector<Detection>& blobs, const Detection& to)
{
  const Detection* best = nullptr;
  float best_d2 = std::numeric_limits<float>::max();
  for(const auto& b : blobs)
  {
    const float dx = b.cx - to.cx, dy = b.cy - to.cy, dz = b.cz - to.cz;
    const float d2 = dx * dx + dy * dy + dz * dz;
    if(d2 < best_d2)
    {
      best_d2 = d2;
      best = &b;
    }
  }
  return best;
}

// Three well-separated balls; the same cloud every case starts from.
std::vector<float> three_balls()
{
  std::mt19937 rng{1234};
  std::vector<float> pos;
  add_ball(pos, 0.f, 0.f, 0.f, 0.20f, 400, rng);
  add_ball(pos, 1.5f, 0.f, 0.f, 0.15f, 300, rng);
  add_ball(pos, 0.f, 1.5f, 0.6f, 0.25f, 500, rng);
  return pos;
}

ClusterParams three_ball_params()
{
  ClusterParams p;
  p.cluster_dist = 0.08f;
  p.min_points = 20;
  p.max_blobs = 100;
  p.knn_k = 6;
  return p;
}

// The GPU quantises centroids against the frame bounding box with a budget of
// 2e9 / valid_points per axis, so the worst-case error on one coordinate is
// extent / budget. This is that bound, not a fitted number.
float centroid_tolerance(const std::vector<float>& tight, int valid_points)
{
  float lo[3]{1e30f, 1e30f, 1e30f}, hi[3]{-1e30f, -1e30f, -1e30f};
  for(std::size_t i = 0; i + 2 < tight.size(); i += 3)
    for(int a = 0; a < 3; a++)
    {
      lo[a] = std::min(lo[a], tight[i + a]);
      hi[a] = std::max(hi[a], tight[i + a]);
    }

  float ext = 0.f;
  for(int a = 0; a < 3; a++)
    ext = std::max(ext, hi[a] - lo[a]);

  const float budget = 2.0e9f / std::max(1.f, (float)valid_points);
  return std::max(ext / budget, 1e-5f) * 4.f;
}
}

TEST_CASE("GPU clustering agrees with the CPU reference", "[threedim][blobs][gpu]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const auto cloud = three_balls();
  const auto params = three_ball_params();
  const int64_t points = (int64_t)(cloud.size() / 3);

  const auto gpu = run_pipeline(backend, cloud, points, 3 * sizeof(float), 0, params);
  if(gpu.skipped)
    SKIP(gpu.backend + ": " + gpu.skip_reason);
  INFO("backend=" << gpu.backend << " reason=" << gpu.skip_reason);
  REQUIRE(gpu.ran);

  const auto cpu = run_cpu(cloud, params);

  CHECK(gpu.result.num_valid_points == cpu.result.num_valid_points);
  CHECK(gpu.result.num_clusters == cpu.result.num_clusters);
  CHECK(gpu.result.cluster_overflow == cpu.result.cluster_overflow);
  CHECK(gpu.blobs.size() == cpu.blobs.size());
  CHECK(sorted_counts(gpu.blobs) == sorted_counts(cpu.blobs));

  const float tol = centroid_tolerance(cloud, cpu.result.num_valid_points);
  CAPTURE(tol);
  for(const auto& expected : cpu.blobs)
  {
    const Detection* got = nearest(gpu.blobs, expected);
    REQUIRE(got != nullptr);

    CHECK(got->point_count == expected.point_count);
    CHECK(got->cx == Approx(expected.cx).margin(tol));
    CHECK(got->cy == Approx(expected.cy).margin(tol));
    CHECK(got->cz == Approx(expected.cz).margin(tol));

    // The bounding box is a plain atomic min/max of the coordinates on both
    // sides: no quantisation, so it must agree exactly.
    CHECK(got->bmnx == Approx(expected.bmnx));
    CHECK(got->bmny == Approx(expected.bmny));
    CHECK(got->bmnz == Approx(expected.bmnz));
    CHECK(got->bmxx == Approx(expected.bmxx));
    CHECK(got->bmxy == Approx(expected.bmxy));
    CHECK(got->bmxz == Approx(expected.bmxz));
  }

  // Both measure the same cloud with the same k and the same percentile, but
  // NOT over the same 256 samples: the GPU's scatter assigns slots within a
  // cell by atomicAdd, so its sorted order — and therefore which points the
  // strided sampler lands on — differs from the CPU's stable scatter. On an
  // irregular cloud the 6th-neighbour distance genuinely varies from point to
  // point, so two different subsamples give two slightly different 75th
  // percentiles. The tight agreement is asserted on a lattice below, where the
  // quantity is well conditioned; here the claim is only that both measured the
  // same cloud.
  CHECK(gpu.result.knn_spacing > 0.f);
  CHECK(gpu.result.knn_spacing == Approx(cpu.result.knn_spacing).epsilon(0.15));
}

TEST_CASE("GPU and CPU measure the same point spacing", "[threedim][blobs][gpu]")
{
  // On a cubic lattice an interior point's 6 nearest neighbours are all exactly
  // one step away, so the k=6 estimate does not depend on which points were
  // sampled — which makes this the case where the two implementations must
  // actually agree, and where a real difference in the k-NN shader would show.
  constexpr int side = 30;
  constexpr float step = 0.1f;

  std::vector<float> cloud;
  cloud.reserve(side * side * side * 3);
  for(int i = 0; i < side; i++)
    for(int j = 0; j < side; j++)
      for(int k = 0; k < side; k++)
      {
        cloud.push_back(i * step);
        cloud.push_back(j * step);
        cloud.push_back(k * step);
      }

  ClusterParams params;
  params.cluster_dist = step * 1.2f;
  params.min_points = 10;
  params.max_blobs = 100;
  params.knn_k = 6;

  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const auto gpu = run_pipeline(
      backend, cloud, (int64_t)(cloud.size() / 3), 3 * sizeof(float), 0, params);
  if(gpu.skipped)
    SKIP(gpu.backend + ": " + gpu.skip_reason);
  REQUIRE(gpu.ran);

  const auto cpu = run_cpu(cloud, params);

  CHECK(gpu.result.knn_spacing == Approx(step).epsilon(0.01));
  CHECK(gpu.result.knn_spacing == Approx(cpu.result.knn_spacing).epsilon(0.01));

  // One lattice is one connected component under a 1.2-step cluster distance.
  CHECK(gpu.blobs.size() == 1);
  CHECK(gpu.blobs.size() == cpu.blobs.size());
  CHECK(gpu.result.num_valid_points == side * side * side);
}

TEST_CASE(
    "GPU clustering reads positions in place from an interleaved buffer",
    "[threedim][blobs][gpu]")
{
  // The port reads through a stride/offset so a geometry's own vertex buffer
  // can be clustered without a repacking pass in front of it — the upstream
  // POP needed a tightly packed P attribute. Same cloud, interleaved as
  // {normal, position} so the position sits at a non-zero offset.
  const auto tight = three_balls();
  const auto params = three_ball_params();
  const int64_t points = (int64_t)(tight.size() / 3);

  std::vector<float> interleaved;
  interleaved.reserve(points * 6);
  for(int64_t i = 0; i < points; i++)
  {
    interleaved.push_back(-1.f); // decoy: if the offset were ignored, these
    interleaved.push_back(-2.f); // would cluster into one giant blob at
    interleaved.push_back(-3.f); // (-1,-2,-3) instead
    interleaved.push_back(tight[i * 3 + 0]);
    interleaved.push_back(tight[i * 3 + 1]);
    interleaved.push_back(tight[i * 3 + 2]);
  }

  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const auto gpu = run_pipeline(
      backend, interleaved, points, 6 * sizeof(float), 3 * sizeof(float), params);
  if(gpu.skipped)
    SKIP(gpu.backend + ": " + gpu.skip_reason);
  INFO("backend=" << gpu.backend << " reason=" << gpu.skip_reason);
  REQUIRE(gpu.ran);

  const auto cpu = run_cpu(tight, params);
  CHECK(gpu.result.num_valid_points == points);
  CHECK(gpu.blobs.size() == cpu.blobs.size());
  CHECK(sorted_counts(gpu.blobs) == sorted_counts(cpu.blobs));
}

TEST_CASE("GPU clustering skips non-finite points", "[threedim][blobs][gpu]")
{
  // A NaN operand makes the atomic min/max compare-and-swap loops in the bbox
  // and accumulate passes spin forever — a GPU hang, not a wrong number. This
  // is the case that has to keep working on every backend.
  std::mt19937 rng{7};
  std::vector<float> cloud;
  add_ball(cloud, 0.f, 0.f, 0.f, 0.1f, 200, rng);

  const auto nan = std::numeric_limits<float>::quiet_NaN();
  const auto inf = std::numeric_limits<float>::infinity();
  for(auto bad : {nan, inf, -inf, 1e20f})
  {
    cloud.push_back(bad);
    cloud.push_back(0.f);
    cloud.push_back(0.f);
  }
  add_ball(cloud, 3.f, 0.f, 0.f, 0.1f, 150, rng);

  ClusterParams params;
  params.cluster_dist = 0.06f;
  params.min_points = 10;
  params.max_blobs = 100;

  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const auto gpu = run_pipeline(
      backend, cloud, (int64_t)(cloud.size() / 3), 3 * sizeof(float), 0, params);
  if(gpu.skipped)
    SKIP(gpu.backend + ": " + gpu.skip_reason);
  REQUIRE(gpu.ran);

  CHECK(gpu.result.num_valid_points == 350);
  REQUIRE(gpu.blobs.size() == 2);
  for(const auto& b : gpu.blobs)
  {
    CHECK(std::isfinite(b.cx));
    CHECK(std::isfinite(b.bmnx));
    CHECK(std::isfinite(b.bmxx));
  }
  CHECK(gpu.blobs[0].point_count + gpu.blobs[1].point_count == 350);
}

TEST_CASE("GPU clustering honours Min points and Max blobs", "[threedim][blobs][gpu]")
{
  std::mt19937 rng{3};
  std::vector<float> cloud;
  for(int i = 0; i < 10; i++)
    add_ball(cloud, i * 3.f, 0.f, 0.f, 0.1f, 100, rng);
  add_ball(cloud, -5.f, 0.f, 0.f, 0.1f, 8, rng); // below Min points

  ClusterParams params;
  params.cluster_dist = 0.06f;
  params.min_points = 20;
  params.max_blobs = 4;

  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const auto gpu = run_pipeline(
      backend, cloud, (int64_t)(cloud.size() / 3), 3 * sizeof(float), 0, params);
  if(gpu.skipped)
    SKIP(gpu.backend + ": " + gpu.skip_reason);
  REQUIRE(gpu.ran);

  CHECK(gpu.blobs.size() == 4);
  CHECK(gpu.result.num_blobs == 4);
  for(const auto& b : gpu.blobs)
    CHECK(b.point_count >= 20);

  // The small ball is a component even though it is not a blob.
  CHECK(gpu.result.num_clusters >= 11);
}

TEST_CASE("Every frame clears the accumulators it depends on", "[threedim][blobs][gpu]")
{
  // The pipeline's per-frame clear pass is the piece a single-frame test cannot
  // see: a freshly created QRhiBuffer has UNDEFINED contents, and on most
  // drivers that happens to be zero — which is the correct starting value for
  // the histogram and the counters, so frame 1 looks right even if nothing was
  // cleared. Frame 2 is where it shows: an uncleared histogram doubles, and the
  // scatter then writes past the end of the sorted-position buffer.
  //
  // Running the same cloud N times must give byte-identical results.
  const auto cloud = three_balls();
  const auto params = three_ball_params();
  const int64_t points = (int64_t)(cloud.size() / 3);

  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  std::vector<GpuRun> frames;
  const auto gpu = run_pipeline(
      backend, cloud, points, 3 * sizeof(float), 0, params, /* frames */ 4, &frames);
  if(gpu.skipped)
    SKIP(gpu.backend + ": " + gpu.skip_reason);
  INFO("backend=" << gpu.backend << " reason=" << gpu.skip_reason);
  REQUIRE(gpu.ran);
  REQUIRE(frames.size() == 4);

  for(std::size_t f = 1; f < frames.size(); f++)
  {
    CAPTURE(f);
    CHECK(frames[f].result.num_valid_points == frames[0].result.num_valid_points);
    CHECK(frames[f].result.num_clusters == frames[0].result.num_clusters);
    CHECK(frames[f].result.num_blobs == frames[0].result.num_blobs);
    CHECK(frames[f].result.cluster_overflow == frames[0].result.cluster_overflow);
    REQUIRE(frames[f].blobs.size() == frames[0].blobs.size());

    for(std::size_t i = 0; i < frames[f].blobs.size(); i++)
    {
      const Detection* a = nearest(frames[0].blobs, frames[f].blobs[i]);
      REQUIRE(a != nullptr);
      CHECK(frames[f].blobs[i].point_count == a->point_count);
      CHECK(frames[f].blobs[i].bmnx == Approx(a->bmnx));
      CHECK(frames[f].blobs[i].bmxx == Approx(a->bmxx));
    }
  }
}

TEST_CASE(
    "The per-frame bounding box reduction starts from infinity",
    "[threedim][blobs][gpu]")
{
  // Directly observable proof that the clear pass ran: with the cloud entirely
  // in POSITIVE space, a bbox reduction seeded from +FLT_MAX/-FLT_MAX reports
  // the cloud's true minimum, while one seeded from an uncleared (zero) buffer
  // reports 0. The three-ball cloud straddles the origin, which is exactly why
  // it cannot tell the two apart — so this cloud is offset away from it.
  std::mt19937 rng{31};
  std::vector<float> cloud;
  add_ball(cloud, 10.f, 20.f, 30.f, 0.2f, 400, rng);
  add_ball(cloud, 12.f, 20.f, 30.f, 0.2f, 400, rng);

  ClusterParams params;
  params.cluster_dist = 0.08f;
  params.min_points = 20;
  params.max_blobs = 100;
  params.knn_k = 6;

  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const auto gpu = run_pipeline(
      backend, cloud, (int64_t)(cloud.size() / 3), 3 * sizeof(float), 0, params);
  if(gpu.skipped)
    SKIP(gpu.backend + ": " + gpu.skip_reason);
  REQUIRE(gpu.ran);

  const auto cpu = run_cpu(cloud, params);

  for(int a = 0; a < 3; a++)
  {
    CAPTURE(a);
    CHECK(gpu.result.bounds_min[a] == Approx(cpu.result.bounds_min[a]));
    CHECK(gpu.result.bounds_max[a] == Approx(cpu.result.bounds_max[a]));
    // The claim that makes this test worth having: nowhere near zero.
    CHECK(gpu.result.bounds_min[a] > 9.f);
  }

  // And the blob boxes themselves, which are the same reduction per cluster.
  REQUIRE(gpu.blobs.size() == 2);
  for(const auto& b : gpu.blobs)
  {
    CHECK(b.bmnx > 9.f);
    CHECK(b.bmny > 19.f);
    CHECK(b.bmnz > 29.f);
  }
}
