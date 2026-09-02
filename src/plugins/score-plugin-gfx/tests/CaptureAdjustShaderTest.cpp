// Runs the real adjustCapture GLSL on a GPU and compares it, texel by texel,
// against adjustCaptureReference.
//
// CaptureAdjustTest checks the CPU reference and the UBO packing, which is
// everything except the part that actually draws. The two implementations are
// written to be the same maths, and "written to be" is exactly the claim that
// rots: this is what makes a divergence fail a build instead of appearing as a
// colour shift somebody eventually notices on a camera.
//
// Skips rather than fails when no GPU is reachable -- a headless CI box with no
// GL is not a broken shader. Backend via SCORE_TEST_API=vulkan|opengl.

#include <Gfx/Graph/CaptureAdjust.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Graph/decoders/CaptureAdjustGLSL.hpp>

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QtGlobal>

#include <QtGui/private/qrhi_p.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
// Before 6.6 the rhi/ headers do not exist and qrhi_p.h declares only the
// base QRhiInitParams; the concrete ones live in the per-backend headers.
#include <QtGui/private/qrhigles2_p.h>
#endif
#include <QtGui/private/qrhigles2_p.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

using namespace score::gfx;

namespace
{
int g_fail = 0;
int g_checks = 0;

void check(bool ok, const std::string& what)
{
  ++g_checks;
  if(!ok)
  {
    ++g_fail;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

// A full-screen triangle generated from the vertex index: no mesh, no vertex
// buffers, nothing to get wrong between this test and the thing under test.
const char* kVert = R"(#version 450
layout(location = 0) out vec2 v_texcoord;
void main() {
  v_texcoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(v_texcoord * 2.0 - 1.0, 0.0, 1.0);
}
)";

QString fragSource()
{
  return QString(R"(#version 450

)") + SCORE_GFX_CAPTURE_UNIFORMS + SCORE_GFX_CAPTURE_ADJUST_FN + R"(
layout(binding = 3) uniform sampler2D u_tex;
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() {
  vec3 c = texture(u_tex, v_texcoord).rgb;
  fragColor = vec4(adjustCapture(c), 1.0);
}
)";
}

/// The colours swept through the shader. Deliberately includes the endpoints
/// and values either side of a black level, since those are where the
/// renormalisation and the clamps live.
std::vector<std::array<std::uint8_t, 3>> testColours()
{
  std::vector<std::array<std::uint8_t, 3>> out;
  for(int r : {0, 13, 32, 64, 128, 191, 254, 255})
    for(int g : {0, 51, 128, 200, 255})
      for(int b : {0, 77, 128, 255})
        out.push_back(
            {std::uint8_t(r), std::uint8_t(g), std::uint8_t(b)});
  return out;
}

/// One (settings, colours) sweep: upload, draw, read back, compare.
void runCase(
    QRhi& rhi, GraphicsApi api, QShaderVersion version,
    const CaptureAdjust& adjust, const char* label)
{
  const auto colours = testColours();
  const int w = int(colours.size());
  const int h = 1;

  // Input: one texel per colour, NEAREST so the sample is the byte we wrote.
  std::unique_ptr<QRhiTexture> tex{
      rhi.newTexture(QRhiTexture::RGBA8, QSize{w, h}, 1)};
  if(!tex->create())
  {
    std::printf("  SKIP %s: no RGBA8 texture\n", label);
    return;
  }
  std::unique_ptr<QRhiSampler> sampler{rhi.newSampler(
      QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge)};
  sampler->create();

  QImage src(w, h, QImage::Format_RGBA8888);
  for(int x = 0; x < w; ++x)
    src.setPixelColor(
        x, 0, QColor(colours[x][0], colours[x][1], colours[x][2]));

  std::unique_ptr<QRhiTexture> target{rhi.newTexture(
      QRhiTexture::RGBA8, QSize{w, h}, 1, QRhiTexture::RenderTarget)};
  target->create();
  std::unique_ptr<QRhiTextureRenderTarget> rt{
      rhi.newTextureRenderTarget({{target.get()}})};
  std::unique_ptr<QRhiRenderPassDescriptor> rp{
      rt->newCompatibleRenderPassDescriptor()};
  rt->setRenderPassDescriptor(rp.get());
  rt->create();

  CaptureMaterialUBO ubo{};
  ubo.textureSize[0] = float(w);
  ubo.textureSize[1] = float(h);
  applyTo(adjust, ubo);

  std::unique_ptr<QRhiBuffer> matBuf{rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
      sizeof(CaptureMaterialUBO))};
  matBuf->create();

  std::unique_ptr<QRhiShaderResourceBindings> srb{rhi.newShaderResourceBindings()};
  srb->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           2, QRhiShaderResourceBinding::FragmentStage, matBuf.get()),
       QRhiShaderResourceBinding::sampledTexture(
           3, QRhiShaderResourceBinding::FragmentStage, tex.get(),
           sampler.get())});
  srb->create();

  auto [vs, vsErr]
      = ShaderCache::get(api, version, QByteArray{kVert}, QShader::VertexStage);
  auto [fs, fsErr] = ShaderCache::get(
      api, version, fragSource().toUtf8(), QShader::FragmentStage);
  check(vsErr.isEmpty(), std::string{label} + ": vertex shader bakes");
  check(fsErr.isEmpty(), std::string{label} + ": fragment shader bakes");
  if(!vsErr.isEmpty() || !fsErr.isEmpty())
  {
    std::printf("    %s\n", qPrintable(fsErr));
    return;
  }

  std::unique_ptr<QRhiGraphicsPipeline> ps{rhi.newGraphicsPipeline()};
  ps->setShaderStages(
      {{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
  ps->setVertexInputLayout({}); // the triangle comes from gl_VertexIndex
  ps->setShaderResourceBindings(srb.get());
  ps->setRenderPassDescriptor(rp.get());
  ps->setTopology(QRhiGraphicsPipeline::Triangles);
  if(!ps->create())
  {
    std::printf("  SKIP %s: pipeline creation failed\n", label);
    return;
  }

  QRhiCommandBuffer* cb{};
  if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
  {
    std::printf("  SKIP %s: no offscreen frame\n", label);
    return;
  }

  auto* batch = rhi.nextResourceUpdateBatch();
  batch->uploadTexture(tex.get(), src);
  batch->updateDynamicBuffer(matBuf.get(), 0, sizeof(CaptureMaterialUBO), &ubo);

  cb->beginPass(rt.get(), QColor::fromRgbF(0, 0, 0, 1), {1.f, 0}, batch);
  cb->setGraphicsPipeline(ps.get());
  cb->setViewport({0, 0, float(w), float(h)});
  cb->setShaderResources(srb.get());
  cb->draw(3);
  cb->endPass();

  QRhiReadbackResult rb;
  bool done = false;
  rb.completed = [&done] { done = true; };
  auto* readBatch = rhi.nextResourceUpdateBatch();
  readBatch->readBackTexture({target.get()}, &rb);
  cb->resourceUpdate(readBatch);
  rhi.endOffscreenFrame();

  if(!done || rb.data.isEmpty())
  {
    std::printf("  SKIP %s: readback produced nothing\n", label);
    return;
  }

  const auto* px = reinterpret_cast<const std::uint8_t*>(rb.data.constData());
  int worst = 0;
  int worstX = -1;
  for(int x = 0; x < w; ++x)
  {
    // The reference is fed the same quantised bytes the shader sampled.
    float ref[3]{
        colours[x][0] / 255.f, colours[x][1] / 255.f, colours[x][2] / 255.f};
    adjustCaptureReference(adjust, ref);

    for(int c = 0; c < 3; ++c)
    {
      const int got = px[x * 4 + c];
      const int want = int(std::lround(ref[c] * 255.f));
      const int diff = std::abs(got - want);
      if(diff > worst)
      {
        worst = diff;
        worstX = x;
      }
    }
  }

  // Two 8-bit quantisations (the render target, and the reference's rounding)
  // plus whatever precision the driver's pow() has. Anything past this is a
  // difference in the maths, not in the arithmetic.
  const bool ok = worst <= 2;
  check(ok, std::string{label} + ": shader matches the reference");
  std::printf(
      "  %-28s worst channel delta %d/255%s\n", label, worst,
      worstX >= 0 && !ok
          ? (" at colour " + std::to_string(worstX)).c_str()
          : "");
}

/// Links the *real* pair of shaders a capture decoder uses.
///
/// The comparison above uses a bespoke vertex shader with no material block, so
/// it cannot see a stage mismatch: a fragment stage with the long material block
/// against a vertex stage with the short one fails to link -- "struct type
/// mismatch between shaders for uniform (named mat)" -- and a capture then goes
/// black with no shader error, because the failure is at link time.
void checkRealProgramLinks(QRhi& rhi, GraphicsApi api, QShaderVersion version)
{
  auto [vs, vsErr] = ShaderCache::get(
      api, version, QByteArray{captureVertexShader}, QShader::VertexStage);
  auto [fs, fsErr] = ShaderCache::get(
      api, version, fragSource().toUtf8(), QShader::FragmentStage);
  check(vsErr.isEmpty(), "capture vertex shader bakes");
  check(fsErr.isEmpty(), "capture fragment shader bakes");
  if(!vsErr.isEmpty() || !fsErr.isEmpty())
    return;

  std::unique_ptr<QRhiTexture> tex{
      rhi.newTexture(QRhiTexture::RGBA8, QSize{4, 4}, 1)};
  tex->create();
  std::unique_ptr<QRhiSampler> sampler{rhi.newSampler(
      QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge)};
  sampler->create();
  std::unique_ptr<QRhiTexture> target{rhi.newTexture(
      QRhiTexture::RGBA8, QSize{4, 4}, 1, QRhiTexture::RenderTarget)};
  target->create();
  std::unique_ptr<QRhiTextureRenderTarget> rt{
      rhi.newTextureRenderTarget({{target.get()}})};
  std::unique_ptr<QRhiRenderPassDescriptor> rp{
      rt->newCompatibleRenderPassDescriptor()};
  rt->setRenderPassDescriptor(rp.get());
  rt->create();

  // Both blocks the real shaders declare, visible to both stages -- the vertex
  // stage reads renderer.clipSpaceCorrMatrix and mat.scale.
  std::unique_ptr<QRhiBuffer> rendererBuf{rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 128)};
  rendererBuf->create();
  std::unique_ptr<QRhiBuffer> matBuf{rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
      sizeof(CaptureMaterialUBO))};
  matBuf->create();

  const auto stages = QRhiShaderResourceBinding::VertexStage
                      | QRhiShaderResourceBinding::FragmentStage;
  std::unique_ptr<QRhiShaderResourceBindings> srb{rhi.newShaderResourceBindings()};
  srb->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(0, stages, rendererBuf.get()),
       QRhiShaderResourceBinding::uniformBuffer(2, stages, matBuf.get()),
       QRhiShaderResourceBinding::sampledTexture(
           3, QRhiShaderResourceBinding::FragmentStage, tex.get(),
           sampler.get())});
  srb->create();

  QRhiVertexInputLayout layout;
  layout.setBindings({QRhiVertexInputBinding{4 * sizeof(float)}});
  layout.setAttributes(
      {QRhiVertexInputAttribute{0, 0, QRhiVertexInputAttribute::Float2, 0},
       QRhiVertexInputAttribute{
           0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}});

  std::unique_ptr<QRhiGraphicsPipeline> ps{rhi.newGraphicsPipeline()};
  ps->setShaderStages(
      {{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
  ps->setVertexInputLayout(layout);
  ps->setShaderResourceBindings(srb.get());
  ps->setRenderPassDescriptor(rp.get());
  ps->setTopology(QRhiGraphicsPipeline::TriangleStrip);

  // On GL this is where the program is linked, so a block mismatch between the
  // stages fails right here.
  check(ps->create(), "the real capture vertex/fragment pair links");
}

/// The mismatch this guards against: a vertex stage declaring the SHORT
/// material block against a fragment stage declaring the long one.
///
/// A check that cannot fail proves nothing, so this asserts the mismatch is
/// still rejected. If a future GL driver silently accepted it, the linkage
/// check above would be passing for the wrong reason and this says so.
void checkMismatchIsStillDetected(
    QRhi& rhi, GraphicsApi api, QShaderVersion version)
{
  static const char* shortBlockVert = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(std140, binding = 2) uniform material_t {
  vec2 scale;
  vec2 texSz;
} mat;

out gl_PerVertex { vec4 gl_Position; };
void main() {
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position * mat.scale, 0.0, 1.);
}
)_";

  auto [vs, vsErr] = ShaderCache::get(
      api, version, QByteArray{shortBlockVert}, QShader::VertexStage);
  auto [fs, fsErr] = ShaderCache::get(
      api, version, fragSource().toUtf8(), QShader::FragmentStage);
  if(!vsErr.isEmpty() || !fsErr.isEmpty())
    return;

  std::unique_ptr<QRhiTexture> tex{
      rhi.newTexture(QRhiTexture::RGBA8, QSize{4, 4}, 1)};
  tex->create();
  std::unique_ptr<QRhiSampler> sampler{rhi.newSampler(
      QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge)};
  sampler->create();
  std::unique_ptr<QRhiTexture> target{rhi.newTexture(
      QRhiTexture::RGBA8, QSize{4, 4}, 1, QRhiTexture::RenderTarget)};
  target->create();
  std::unique_ptr<QRhiTextureRenderTarget> rt{
      rhi.newTextureRenderTarget({{target.get()}})};
  std::unique_ptr<QRhiRenderPassDescriptor> rp{
      rt->newCompatibleRenderPassDescriptor()};
  rt->setRenderPassDescriptor(rp.get());
  rt->create();

  std::unique_ptr<QRhiBuffer> rendererBuf{
      rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 128)};
  rendererBuf->create();
  std::unique_ptr<QRhiBuffer> matBuf{rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
      sizeof(CaptureMaterialUBO))};
  matBuf->create();

  const auto stages = QRhiShaderResourceBinding::VertexStage
                      | QRhiShaderResourceBinding::FragmentStage;
  std::unique_ptr<QRhiShaderResourceBindings> srb{rhi.newShaderResourceBindings()};
  srb->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(0, stages, rendererBuf.get()),
       QRhiShaderResourceBinding::uniformBuffer(2, stages, matBuf.get()),
       QRhiShaderResourceBinding::sampledTexture(
           3, QRhiShaderResourceBinding::FragmentStage, tex.get(),
           sampler.get())});
  srb->create();

  QRhiVertexInputLayout layout;
  layout.setBindings({QRhiVertexInputBinding{4 * sizeof(float)}});
  layout.setAttributes(
      {QRhiVertexInputAttribute{0, 0, QRhiVertexInputAttribute::Float2, 0},
       QRhiVertexInputAttribute{
           0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}});

  std::unique_ptr<QRhiGraphicsPipeline> ps{rhi.newGraphicsPipeline()};
  ps->setShaderStages(
      {{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
  ps->setVertexInputLayout(layout);
  ps->setShaderResourceBindings(srb.get());
  ps->setRenderPassDescriptor(rp.get());
  ps->setTopology(QRhiGraphicsPipeline::TriangleStrip);

  std::printf("  (the next GL link error is expected -- negative control)\n");
  check(!ps->create(), "a mismatched material block is still rejected");
}
} // namespace

/// Everything the test does once an app context exists. createRenderState
/// reaches for score::AppContext(), so none of this can run before one does --
/// without it the process segfaults before printing anything.
int runTests()
{
  // The RHI is built here rather than through createRenderState, which reaches
  // for score::AppContext() and therefore needs a whole application booted --
  // resources, settings and all -- to compare two implementations of a
  // fragment shader. Nothing below needs any of that.
  const GraphicsApi api = GraphicsApi::OpenGL;

  // Desktop GL and GLES bake to different GLSL dialects, and the difference is
  // not cosmetic here: the corrections lean on pow() and on mediump-versus-
  // highp defaults. Ask the context which one this is rather than assume, so
  // the same test says something on a Jetson as on a workstation.
  bool isEs = false;
  {
    QOpenGLContext probe;
    if(probe.create())
      isEs = probe.isOpenGLES();
  }
  const QShaderVersion version
      = isEs ? QShaderVersion{300, QShaderVersion::GlslEs} : QShaderVersion{330};

  std::unique_ptr<QOffscreenSurface> surface{
      QRhiGles2InitParams::newFallbackSurface()};
  QRhiGles2InitParams params;
  params.fallbackSurface = surface.get();

  std::unique_ptr<QRhi> rhiOwner{QRhi::create(QRhi::OpenGLES2, &params)};
  if(!rhiOwner)
  {
    // Not a failure: a box with no GPU cannot say anything about a shader.
    std::printf("CaptureAdjustShaderTest: no RHI available, skipping\n");
    return 0;
  }
  auto& rhi = *rhiOwner;

  std::printf(
      "CaptureAdjustShaderTest: %s, %s\n", rhi.backendName(),
      isEs ? "GLSL ES 300" : "GLSL 330");

  checkRealProgramLinks(rhi, api, version);
  checkMismatchIsStillDetected(rhi, api, version);

  // Identity first: if the defaults are not a no-op on the GPU, nothing below
  // means anything.
  runCase(rhi, api, version, CaptureAdjust{}, "identity");

  {
    CaptureAdjust a{};
    a.gamma = 2.2f;
    runCase(rhi, api, version, a, "gamma 2.2");
  }
  {
    CaptureAdjust a{};
    a.whiteBalance[0] = 1.7f;
    a.whiteBalance[2] = 0.6f;
    runCase(rhi, api, version, a, "white balance");
  }
  {
    CaptureAdjust a{};
    a.blackLevel[0] = a.blackLevel[1] = a.blackLevel[2] = 0.2f;
    runCase(rhi, api, version, a, "black level");
  }
  {
    CaptureAdjust a{};
    a.saturation = 0.f;
    runCase(rhi, api, version, a, "saturation 0");
  }
  {
    CaptureAdjust a{};
    a.saturation = 2.f;
    runCase(rhi, api, version, a, "saturation 2");
  }
  {
    // Everything at once, which is where an ordering difference between the
    // two implementations would show up.
    CaptureAdjust a{};
    a.blackLevel[0] = 0.1f;
    a.blackLevel[1] = 0.05f;
    a.blackLevel[2] = 0.15f;
    a.whiteBalance[0] = 1.4f;
    a.whiteBalance[1] = 1.0f;
    a.whiteBalance[2] = 1.8f;
    a.exposure = 1.3f;
    a.gamma = 2.2f;
    a.saturation = 1.2f;
    runCase(rhi, api, version, a, "all together");
  }
  {
    CaptureAdjust a{};
    a.blackLevel[0] = a.blackLevel[1] = a.blackLevel[2] = 1.f;
    a.gamma = 0.f;
    runCase(rhi, api, version, a, "degenerate");
  }

  std::printf("\n%d checks, %d failures\n", g_checks, g_fail);
  return g_fail == 0 ? 0 : 1;
}

int main(int argc, char** argv)
{
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setlocale(LC_ALL, "C");
  QGuiApplication app{argc, argv};
  return runTests();
}
