#pragma once

// =============================================================================
// L3 GPU render + readback fixture for score's gfx render engine.
// =============================================================================
//
// Build a minimal gfx pipeline from one or more ISF/CSF/raw-raster shaders,
// render it offscreen for a fixed number of frames, and hand the RGBA8 pixels
// to a Catch2 test.
//
// The same render is validated on every RHI backend available on the box, so a
// test iterates platform_backends() with GENERATE:
//
//     const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
//     score::test::gfx::IsfResult r;
//     score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
//       r = score::test::gfx::render_isf_chain(backend, {"corpus/isf-solid-color.fs"});
//     });
//     if(r.skipped) SKIP(r.backend << ": " << r.skip_reason);
//     REQUIRE(r.error.empty());
//     CHECK(near(r.outputs[0].center(), {255,0,255,255}, 2));
//
// The GPU work must run on the main thread inside a booted GUI application, so
// collect pixels INSIDE the lambda and run CHECK/REQUIRE/SKIP after
// run_in_gui_app returns: those macros throw, which would bypass the app's
// teardown. render_isf_chain never throws; it reports through IsfResult.
//
// The backend is forced end-to-end by handing the GraphicsApi to
// Graph::createAllRenderLists(api), which flows into each BackgroundNode's
// createOutput and createRenderState. A backend that cannot initialize here
// returns skipped=true rather than failing.
//
// Readback is RGBA8, tightly packed, origin top-left: the offscreen output goes
// through InvertYRenderer. Values can differ by a couple of LSB between
// backends, so tests use `near`.
// =============================================================================

#include <score_test/App.hpp>

#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <Process/Dataflow/CableData.hpp>

#include <Gfx/Graph/Utils.hpp>

#include <ossia/network/value/value.hpp>
#include <ossia/dataflow/texture_port.hpp>

#include <score/gfx/Vulkan.hpp>

#include <isf.hpp>

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QSize>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// <windows.h> defines near/far as empty macros; they mangle any declaration
// that uses those names.
#if defined(_WIN32)
#undef near
#undef far
#endif

namespace score::test::gfx
{

// -----------------------------------------------------------------------------
// A single RGBA8 offscreen readback (tightly packed, origin top-left).
// -----------------------------------------------------------------------------
struct ReadbackImage
{
  int width = 0;
  int height = 0;
  QRhiTexture::Format format = QRhiTexture::RGBA8;
  QByteArray bytes; // width*height*4, row stride == width*4

  bool valid() const noexcept
  {
    return width > 0 && height > 0
           && bytes.size() >= qsizetype(width) * height * 4;
  }

  /// RGBA of the pixel at (x, y). Bounds are the caller's responsibility.
  std::array<uint8_t, 4> at(int x, int y) const noexcept
  {
    const auto* p = reinterpret_cast<const uint8_t*>(bytes.constData())
                    + (qsizetype(y) * width + x) * 4;
    return {p[0], p[1], p[2], p[3]};
  }

  std::array<uint8_t, 4> center() const noexcept { return at(width / 2, height / 2); }
};

// -----------------------------------------------------------------------------
// Result of a render attempt. Never thrown — always returned, so the caller can
// run Catch2 macros outside the app-bootstrap lambda (see header doc).
// -----------------------------------------------------------------------------
struct IsfResult
{
  bool skipped = false;      // no usable RHI backend (CI without a GPU)
  std::string skip_reason;   // human-readable reason for the skip

  std::string error;         // non-empty => a real failure (compile / wiring / empty readback)
  std::string backend;       // QRhi backend name actually used (e.g. "OpenGL", "Vulkan")

  // One entry per image output port of the *last* shader in the chain.
  // A plain shader has 1; an MRT shader (declared OUTPUTS) has N.
  std::vector<ReadbackImage> outputs;
};

// -----------------------------------------------------------------------------
// Small helpers.
// -----------------------------------------------------------------------------

/// Absolute difference within tolerance on every channel.
inline bool
near(std::array<uint8_t, 4> a, std::array<uint8_t, 4> b, int tol) noexcept
{
  for(int i = 0; i < 4; ++i)
    if(std::abs(int(a[i]) - int(b[i])) > tol)
      return false;
  return true;
}

/// Human-readable backend name (for test messages / GENERATE captions).
inline const char* backend_name(score::gfx::GraphicsApi api) noexcept
{
  switch(api)
  {
    case score::gfx::OpenGL:
      return "OpenGL";
    case score::gfx::Vulkan:
      return "Vulkan";
    case score::gfx::D3D11:
      return "D3D11";
    case score::gfx::D3D12:
      return "D3D12";
    case score::gfx::Metal:
      return "Metal";
    case score::gfx::Null:
      return "Null";
    default:
      return "Unknown";
  }
}

/// The RHI backends to exercise on this platform. Not probed: probing needs a
/// booted app and the Gfx settings model, neither of which exists when Catch2
/// evaluates the GENERATE, so this is the platform-plausible set and actual
/// availability is decided inside the app by probe_api. Restrict with
/// SCORE_TEST_API=<name>.
inline std::vector<score::gfx::GraphicsApi> platform_backends()
{
  const QByteArray env = qgetenv("SCORE_TEST_API").toLower();
  if(env == "vulkan" || env == "vk")
    return {score::gfx::Vulkan};
  if(env == "opengl" || env == "gl" || env == "gles")
    return {score::gfx::OpenGL};
  if(env == "metal")
    return {score::gfx::Metal};
  if(env == "d3d11")
    return {score::gfx::D3D11};
  if(env == "d3d12")
    return {score::gfx::D3D12};
  if(env == "null")
    return {score::gfx::Null};

#if defined(__APPLE__)
  return {score::gfx::Metal};
#elif defined(_WIN32)
  return {score::gfx::D3D11, score::gfx::D3D12, score::gfx::Vulkan, score::gfx::OpenGL};
#else
  return {score::gfx::OpenGL, score::gfx::Vulkan};
#endif
}

// -----------------------------------------------------------------------------
// THE NULL BACKEND REFUSES RATHER THAN PRETENDS (spec P2-15).
//
// QRhi's Null backend accepts every command and rasterizes nothing. It is a
// legitimate target for the parts of a case that are DECISIONS -- which shim was
// selected, which pass was recorded, which caps were queried -- and it is not a
// legitimate target for anything that reads a pixel back.
//
// Before this, a pixel case run with SCORE_TEST_API=null went all the way
// through create() and render() and then died on
//     "readback of output 0 was empty/short (got 0 bytes ...)"
// which is a RED that names the fixture rather than the code under test, and
// which a reader triaging a platform run cannot tell from a real regression. The
// symmetric hazard is worse: a case whose expected colour happens to be the zero
// pixel would have gone GREEN against a buffer that was never drawn.
//
// So the fixture refuses: create()/render_isf_chain() report skipped=true with
// this reason as soon as Null is the requested backend, and the standard
//     if(r.skipped) SKIP(r.skip_reason);
// prologue every case already carries turns that into a SKIP verdict.
//
// A case whose assertions are STRUCTURAL -- and which therefore wants to run on
// Null on purpose, e.g. GfxCubemapSixFaces' shim-selection half -- opts back in
// with GfxPipeline::allowNullBackend(). Opting in does not make pixels appear:
// the readback is still empty, and readback().valid() is still false. That is
// the "does not pretend" half, and it is asserted in GfxNullBackendRefuses.cpp.
// -----------------------------------------------------------------------------
inline const char* null_backend_skip_reason() noexcept
{
  return "RHI backend 'Null' accepts every command and rasterizes nothing, so no "
         "pixel assertion can be made against it. The fixture refuses rather than "
         "hand back a buffer that was never drawn; opt in with "
         "GfxPipeline::allowNullBackend() for structural (decision-logic) "
         "assertions only.";
}

/// Try to bring up a QRhi for `api`. Returns true (and the backend name) if a
/// real device could be created, false otherwise. Non-destructive: the probe
/// state is torn down before returning.
inline bool probe_api(score::gfx::GraphicsApi api, std::string& backendName)
{
  // Cache the result per backend for the lifetime of the process. Backend
  // availability doesn't change at runtime, and — importantly — repeatedly
  // asking Qt to create a QRhi that CANNOT initialize (e.g. OpenGL under the
  // bare "offscreen" QPA, which fails temporary-context creation) across the
  // app-boot/teardown cycles of successive test cases can segfault inside Qt's
  // GL fallback-surface cleanup. Probing each backend at most once sidesteps
  // that and keeps the no-GPU path a clean per-backend SKIP.
  struct CacheEntry
  {
    bool probed = false;
    bool ok = false;
    std::string name;
  };
  static CacheEntry cache[8]{};
  const int idx = int(api) & 7;
  if(cache[idx].probed)
  {
    backendName = cache[idx].name;
    return cache[idx].ok;
  }

  // createRenderState(Vulkan, ...) passes score::gfx::staticVulkanInstance()
  // straight into QRhi::create, which dereferences it; under
  // QT_QPA_PLATFORM=offscreen that is null. Guard so an unavailable Vulkan is a
  // clean per-backend SKIP.
#if QT_HAS_VULKAN
  if(api == score::gfx::Vulkan && !score::gfx::staticVulkanInstance())
  {
    cache[idx] = {true, false, backendName};
    return false;
  }
#else
  if(api == score::gfx::Vulkan)
  {
    cache[idx] = {true, false, backendName};
    return false;
  }
#endif

  auto st = score::gfx::createRenderState(api, QSize{16, 16}, nullptr);
  bool ok = st && st->rhi;

  // A Null-backend QRhi is a fallback QRhi, not the requested API: treat it as
  // "backend unavailable" unless Null was explicitly requested.
  if(ok && api != score::gfx::Null
     && st->rhi->backend() == QRhi::Implementation::Null)
    ok = false;

  // Reject a legacy OpenGL context (GLSL < 330). The bare "offscreen" QPA plugin
  // often hands out a GL 2.x context with no working GPU; a QRhi is created, but
  // score's shaders are generated as #version 450 and cannot cross-compile down
  // to GLSL 1.x. Treat it as unusable so the fixture falls through to the next
  // candidate API and SKIPs cleanly.
  if(ok && api == score::gfx::OpenGL && st->version.version() < 330)
    ok = false;

  if(ok)
    backendName = st->rhi->backendName();
  if(st)
    st->destroy();

  cache[idx] = {true, ok, backendName};
  return ok;
}

// -----------------------------------------------------------------------------
// Build a score::gfx::ISFNode from an ISF fragment-shader file on disk.
// (Also picks up a sibling .vs/.vert vertex shader if present, exactly as the
//  score Filter process does.)
// -----------------------------------------------------------------------------
struct BuiltIsf
{
  std::unique_ptr<score::gfx::ISFNode> node;
  std::string error;
};

// -----------------------------------------------------------------------------
// Build a score::gfx::ISFNode from a CSF compute shader (.cs) on disk.
//
// Same ISFNode class as an ISF fragment node, built through its single-QString
// compute constructor and rendered by RenderedCSFNode. CSF does not go through
// Gfx::ProgramCache: the score CSF Model parses the file inline with isf::parser
// in ShaderType::CSF mode, and this mirrors that path exactly.
//
// A CSF declaring a write_only / read_write image RESOURCE exposes a
// Types::Image output, so it plugs into render_isf_chain like an ISF node: the
// compute passes dispatch in runInitialPasses, then a graphics pass blits the
// written image into the sink's RGBA8 target. A single-channel image blits as
// vec4(v,v,v,1), i.e. reads back grayscale.
//
// Geometry / storage-buffer producing CSFs are not drivable here -- see
// csf_geometry_readback_skip_reason() below.
// -----------------------------------------------------------------------------
inline BuiltIsf make_csf_node(const QString& path)
{
  if(!QFile::exists(path))
    return {nullptr, "compute shader file not found: " + path.toStdString()};

  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {nullptr, "cannot open compute shader: " + path.toStdString()};
  const QByteArray raw = f.readAll();

  // Expand #include directives exactly as Gfx::CSF::Model::setScript does.
  auto [resolved, err] = Gfx::preprocessShaderIncludes(raw, path);
  if(!err.isEmpty())
    return {nullptr, "CSF #include error in " + path.toStdString() + ": "
                         + err.toStdString()};

  try
  {
    isf::parser p{
        std::string(resolved.constData(), std::size_t(resolved.size())),
        isf::parser::ShaderType::CSF};
    if(p.mode() != isf::descriptor::CSF)
      return {nullptr,
              "not a valid CSF (COMPUTE_SHADER) shader: " + path.toStdString()};

    isf::descriptor desc = p.data();
    const QString compute = QString::fromStdString(p.compute_shader());

    // ISFNode::createRenderer returns nullptr for a CSF with no PASSES, which
    // would surface later as a null renderer — reject it up front with a clear
    // reason instead.
    if(desc.csf_passes.empty())
      return {nullptr, "CSF declares no PASSES (needs >=1 compute pass): "
                           + path.toStdString()};

    return {std::make_unique<score::gfx::ISFNode>(desc, compute), {}};
  }
  catch(const std::exception& e)
  {
    return {nullptr, "CSF parse error in " + path.toStdString() + ": " + e.what()};
  }
  catch(...)
  {
    return {nullptr, "unknown CSF parse error in " + path.toStdString()};
  }
}

// -----------------------------------------------------------------------------
// Build a score::gfx::ISFNode for a RAW_RASTER_PIPELINE (.vs + .fs) shader.
//
// A vertex shader reading per-vertex attributes from an upstream geometry
// producer plus a fragment shader carrying the MODE: "RAW_RASTER_PIPELINE"
// descriptor. Same ISFNode class, built through the (desc, vertex, fragment)
// constructor so createRenderer dispatches to RenderedRawRasterPipelineNode.
//
// Mirrors Gfx::RenderPipeline::Model::setProgram: ShaderSource{RawRasterPipeline,
// vertex, fragment} -> ProgramCache::get. The node exposes a Types::Geometry
// input a CSF geometry producer feeds, and one Types::Image output per
// FRAGMENT_OUTPUTS entry.
// -----------------------------------------------------------------------------
inline BuiltIsf make_raster_node(const QString& vsPath, const QString& fsPath)
{
  if(!QFile::exists(fsPath))
    return {nullptr, "raw-raster fragment shader not found: " + fsPath.toStdString()};
  if(!QFile::exists(vsPath))
    return {nullptr, "raw-raster vertex shader not found: " + vsPath.toStdString()};

  QFile vsf{vsPath}, fsf{fsPath};
  if(!vsf.open(QIODevice::ReadOnly))
    return {nullptr, "cannot open raw-raster vertex shader: " + vsPath.toStdString()};
  if(!fsf.open(QIODevice::ReadOnly))
    return {nullptr, "cannot open raw-raster fragment shader: " + fsPath.toStdString()};

  Gfx::ShaderSource src{
      Gfx::ShaderSource::ProgramType::RawRasterPipeline, QString::fromUtf8(vsf.readAll()),
      QString::fromUtf8(fsf.readAll())};

  auto [processed, err] = Gfx::ProgramCache::instance().get(src, fsPath);
  if(!err.isEmpty())
    return {nullptr, "raw-raster compile error in " + fsPath.toStdString() + ": "
                         + err.toStdString()};
  if(!processed)
    return {nullptr, "raw-raster program produced no output: " + fsPath.toStdString()};
  if(processed->descriptor.mode != isf::descriptor::RawRaster)
    return {nullptr, "shader did not parse as RAW_RASTER_PIPELINE: "
                         + fsPath.toStdString()};

  return {std::make_unique<score::gfx::ISFNode>(
              processed->descriptor, processed->vertex, processed->fragment),
          {}};
}

// -----------------------------------------------------------------------------
// Build a score::gfx::ISFNode for a VSA (vertex-shader-art) shader from a .vs.
//
// A single vertex shader with a MODE: "VERTEX_SHADER_ART" header emitting
// POINT_COUNT procedural vertices driven by vertexId, with an auto-generated
// fragment stage; built through ProgramCache::get with type VertexShaderArt so
// createRenderer dispatches to SimpleRenderedVSANode. No geometry input; its
// single Types::Image output reads back like any ISF node.
// -----------------------------------------------------------------------------
inline BuiltIsf make_vsa_node(const QString& vsPath)
{
  if(!QFile::exists(vsPath))
    return {nullptr, "VSA vertex shader not found: " + vsPath.toStdString()};

  QFile vsf{vsPath};
  if(!vsf.open(QIODevice::ReadOnly))
    return {nullptr, "cannot open VSA vertex shader: " + vsPath.toStdString()};

  Gfx::ShaderSource src{
      Gfx::ShaderSource::ProgramType::VertexShaderArt, QString::fromUtf8(vsf.readAll()),
      QString{}};

  auto [processed, err] = Gfx::ProgramCache::instance().get(src, vsPath);
  if(!err.isEmpty())
    return {nullptr, "VSA compile error in " + vsPath.toStdString() + ": "
                         + err.toStdString()};
  if(!processed)
    return {nullptr, "VSA program produced no output: " + vsPath.toStdString()};
  if(processed->descriptor.mode != isf::descriptor::VSA)
    return {nullptr, "shader did not parse as VERTEX_SHADER_ART: "
                         + vsPath.toStdString()};

  return {std::make_unique<score::gfx::ISFNode>(
              processed->descriptor, processed->vertex, processed->fragment),
          {}};
}

inline BuiltIsf make_isf_node(const QString& path)
{
  // Dispatch on extension: a .cs file is a CSF compute node (built via the
  // compute constructor); everything else is an ISF fragment node. This lets
  // every chain helper below (render_isf_chain, GfxPipeline::addIsf, ...) drive
  // a CSF image-output node transparently, since both are score::gfx::ISFNode.
  if(path.endsWith(QStringLiteral(".cs"), Qt::CaseInsensitive))
    return make_csf_node(path);

  if(!QFile::exists(path))
    return {nullptr, "shader file not found: " + path.toStdString()};

  Gfx::ShaderSource src = Gfx::programFromISFFragmentShaderPath(path, {});
  auto [processed, err] = Gfx::ProgramCache::instance().get(src, path);
  if(!err.isEmpty())
    return {nullptr, "ISF compile error in " + path.toStdString() + ": " + err.toStdString()};
  if(!processed)
    return {nullptr, "ISF program produced no output: " + path.toStdString()};

  return {std::make_unique<score::gfx::ISFNode>(
              processed->descriptor, processed->vertex, processed->fragment),
          {}};
}

// -----------------------------------------------------------------------------
// CONTROL-VALUE injection.
//
// An ISF float/color/point2D/point3D/bool/long/event INPUT becomes a control
// inlet: isf_input_port_vis packs it into the node's std140 material buffer and
// creates a Port whose `value` points at that slot. setControl() drives the same
// public entry point the exec engine uses, node.process(port, value), which
// bumps materialChange() so the renderer re-uploads on the next update().
//
// Call after create()/createAllRenderLists and before render(). The value
// persists across subsequent token-only frames.
//
// `inputPortIndex` is the raw index into node->input -- image/geometry/audio
// inlets occupy slots too, so use nth_control_input() to skip them. `value` is
// an ossia::value converted by the engine's value_visitor, so the mapping
// matches production exactly.
// -----------------------------------------------------------------------------
inline void
setControl(score::gfx::ISFNode& node, int inputPortIndex, const ossia::value& v)
{
  // ISFNode overrides process(Message&&), which hides the base per-port
  // overloads in its scope; reach the (public, virtual) control-port entry
  // point on the ProcessNode base explicitly. This is the SAME method
  // ProcessNode::process(Message&&) dispatches each control input to.
  static_cast<score::gfx::ProcessNode&>(node).process(int32_t(inputPortIndex), v);
}

// -----------------------------------------------------------------------------
// RENDER-TARGET-SPEC injection.
//
// A node's image INPUT port can carry an ossia::render_target_spec that pins the
// size / format / sampler-filter / address-mode of the render target the
// UPSTREAM producer draws into (and this node samples). In the running app the
// exec engine delivers it as a score::gfx::Message; Node::process(port, spec)
// stores it in renderTargetSpecs and bumps renderTargetChange(). On the next
// RenderList::render() the renderer's checkForChanges() sees the bump and sets
// renderTargetSpecsChanged, which drives the SURGICAL rt_changed path
// (RenderList.cpp:1060) — recreating the input RT + re-adding the affected
// passes WITHOUT a full rebuild.
//
// setRenderTargetSpec() drives exactly that public entry point
// (Node::process(int32_t, render_target_spec)). Call it AFTER create() and
// BETWEEN render() calls: the change is picked up at the next render(). Use it to
// mutate an intermediate node's input RT shape/format or its sampler
// filter/address. `port` is the RAW index into node->input (use first_image_input /
// nth_image_input to locate an image port).
// -----------------------------------------------------------------------------
inline void setRenderTargetSpec(
    score::gfx::Node& node, int port, const ossia::render_target_spec& v)
{
  // Reach the (public) Node overload explicitly: ISFNode's process(Message&&)
  // / per-port overrides hide the base render_target_spec overload in its scope.
  static_cast<score::gfx::Node&>(node).process(int32_t(port), v);
}

// -----------------------------------------------------------------------------
// AUDIO injection.
//
// An ISF TYPE:"audio" INPUT becomes a Types::Audio port whose `value` points at
// an AudioTexture the node owns; updateAudioTexture uploads it into an R32F
// sampler2D of (samples x channels). On the temporal path a sample s is stored
// as texel 0.5 + s/2, so a constant buffer is analytic through a shader that
// emits the sampled value.
//
// setAudio() drives the same entry point the exec engine uses; call it after
// create() and before render(). audioFFT / audioHist run an FFT or histogram
// over the buffer and are not asserted analytically.
// -----------------------------------------------------------------------------
inline void
setAudio(score::gfx::ISFNode& node, int inputPortIndex, const ossia::audio_vector& v)
{
  static_cast<score::gfx::ProcessNode&>(node).process(int32_t(inputPortIndex), v);
}

/// A single-channel constant audio buffer of `samples` samples all equal to
/// `value` (in [-1,1]). Fed into a TYPE:"audio" inlet it uploads a flat R32F
/// texture whose every texel is 0.5 + value/2.
inline ossia::audio_vector const_audio(double value, int samples)
{
  ossia::audio_vector v;
  v.resize(1);
  v[0].resize(samples);
  for(int i = 0; i < samples; ++i)
    v[0][i] = value;
  return v;
}

/// Index into node->input of the k-th (0-based) *control* inlet — i.e. skipping
/// Image / Geometry / Audio / Buffer ports so callers can address ISF controls
/// by their control ordinal regardless of interleaved texture inputs. Returns -1
/// if there are fewer than k+1 control inlets.
inline int nth_control_input(const score::gfx::Node& n, int k) noexcept
{
  using score::gfx::Types;
  int seen = 0;
  for(std::size_t i = 0; i < n.input.size(); ++i)
  {
    switch(n.input[i]->type)
    {
      case Types::Int:
      case Types::Float:
      case Types::Vec2:
      case Types::Vec3:
      case Types::Vec4:
        if(seen++ == k)
          return int(i);
        break;
      default:
        break;
    }
  }
  return -1;
}

/// Index of the first Image input port of a node, or -1 if it has none.
inline int first_image_input(const score::gfx::Node& n) noexcept
{
  for(std::size_t i = 0; i < n.input.size(); ++i)
    if(n.input[i]->type == score::gfx::Types::Image)
      return int(i);
  return -1;
}

/// The k-th (0-based) Image input port of a node, or nullptr if it has fewer
/// than k+1. A node can have non-image inputs interleaved (e.g. a persistent
/// storage buffer sits in n.input too) — this skips them so callers index by
/// *image* port, matching how a shader's image inputs are numbered.
inline score::gfx::Port* nth_image_input(score::gfx::Node& n, int k) noexcept
{
  int seen = 0;
  for(auto* p : n.input)
    if(p->type == score::gfx::Types::Image)
      if(seen++ == k)
        return p;
  return nullptr;
}

/// The k-th (0-based) Image output port of a node, or nullptr. A plain shader
/// has one; an MRT shader has one per declared OUTPUTS entry.
inline score::gfx::Port* nth_image_output(score::gfx::Node& n, int k) noexcept
{
  int seen = 0;
  for(auto* p : n.output)
    if(p->type == score::gfx::Types::Image)
      if(seen++ == k)
        return p;
  return nullptr;
}

/// The k-th (0-based) Geometry input port of a node, or nullptr. A raw-raster
/// node has exactly one (input[0]); a CSF geometry read/write node also has one.
inline score::gfx::Port* nth_geometry_input(score::gfx::Node& n, int k) noexcept
{
  int seen = 0;
  for(auto* p : n.input)
    if(p->type == score::gfx::Types::Geometry)
      if(seen++ == k)
        return p;
  return nullptr;
}

/// The k-th (0-based) Geometry output port of a node, or nullptr. A CSF geometry
/// producer (write_only / read_write geometry RESOURCE) exposes one.
inline score::gfx::Port* nth_geometry_output(score::gfx::Node& n, int k) noexcept
{
  int seen = 0;
  for(auto* p : n.output)
    if(p->type == score::gfx::Types::Geometry)
      if(seen++ == k)
        return p;
  return nullptr;
}

/// The k-th (0-based) Buffer input port of a node, or nullptr. An ISF
/// `uniform_input` and a read_only `storage_input` both land here — the
/// UniformBuffer flag is what tells them apart, not the port type.
inline score::gfx::Port* nth_buffer_input(score::gfx::Node& n, int k) noexcept
{
  int seen = 0;
  for(auto* p : n.input)
    if(p->type == score::gfx::Types::Buffer)
      if(seen++ == k)
        return p;
  return nullptr;
}

/// The k-th (0-based) Buffer output port of a node, or nullptr. A write_only or
/// read_write `storage` RESOURCE on a CSF exposes one.
inline score::gfx::Port* nth_buffer_output(score::gfx::Node& n, int k) noexcept
{
  int seen = 0;
  for(auto* p : n.output)
    if(p->type == score::gfx::Types::Buffer)
      if(seen++ == k)
        return p;
  return nullptr;
}

/// Deliver one Timings message to each processing node, then render each sink
/// once. Factored out so the linear-chain renderer and the general GfxPipeline
/// driver pump frames identically. `f` is the 0-based frame index in a run of
/// `frames` frames (fed into the node's standard UBO as date/parent_duration).
inline void pump_frame(
    const std::vector<score::gfx::Node*>& processNodes,
    const std::vector<score::gfx::OutputNode*>& sinks, int f, int frames)
{
  score::gfx::Timings tk{};
  tk.date = ossia::time_value{int64_t(f)};
  tk.parent_duration = ossia::time_value{int64_t(frames)};

  for(auto* n : processNodes)
  {
    score::gfx::Message m;
    m.node_id = n->nodeId;
    m.token = tk;
    n->process(std::move(m));
  }
  for(auto* s : sinks)
    s->render();
}

// -----------------------------------------------------------------------------
// Render a linear chain of ISF fragment shaders offscreen and read back the
// final node's image output(s).
//
//   paths[0]      : the source, taking no image input
//   paths[i]      : samples paths[i-1]'s output on its first image input
//   paths.back()  : each of its image OUTPUT ports is read back into its own
//                   offscreen BackgroundNode
//
// size / frames control the offscreen target and how many frames are pumped
// before the readback; frames >= 2 so pipelines are warm.
// -----------------------------------------------------------------------------
inline IsfResult render_isf_chain(
    score::gfx::GraphicsApi backend, std::vector<QString> paths,
    QSize size = {64, 64}, int frames = 3,
    score::gfx::SharedDeviceMode deviceMode = score::gfx::SharedDeviceMode::Owned)
{
  IsfResult r;
  r.backend = backend_name(backend);
  const bool trace = qEnvironmentVariableIsSet("GFX_TRACE");
#define GFX_TR(x)                                       \
  do                                                    \
  {                                                     \
    if(trace)                                           \
    {                                                   \
      std::fprintf(stderr, "GFX_TRACE: %s\n", x);       \
      std::fflush(stderr);                              \
    }                                                   \
  } while(0)

  if(paths.empty())
  {
    r.error = "render_isf_chain: no shader paths given";
    return r;
  }

  // 1. The backend is dictated by the caller. If it cannot initialize on this
  //    machine (no ICD/driver, wrong platform, or a legacy GL context), SKIP
  //    this backend specifically — the caller iterates all backends and each
  //    reports its own availability.
  const score::gfx::GraphicsApi api = backend;

  // Null draws nothing: every caller of this function reads pixels back, so the
  // only honest verdict is SKIP. See null_backend_skip_reason().
  if(api == score::gfx::Null)
  {
    r.skipped = true;
    r.skip_reason = null_backend_skip_reason();
    return r;
  }
  {
    std::string probed;
    if(!probe_api(api, probed))
    {
      r.skipped = true;
      r.skip_reason = std::string("RHI backend '") + backend_name(api)
                      + "' cannot initialize on this machine (no driver/ICD, "
                        "wrong platform, or a legacy/unsupported GL context)";
      return r;
    }
  }

  // 2. Build the ISF node chain.
  std::vector<std::unique_ptr<score::gfx::ISFNode>> isf;
  isf.reserve(paths.size());
  for(const auto& p : paths)
  {
    auto built = make_isf_node(p);
    if(!built.node)
    {
      r.error = built.error;
      return r;
    }
    isf.push_back(std::move(built.node));
  }

  GFX_TR("nodes built");
  auto* last = isf.back().get();

  // Image output ports of the final node (1 for a plain shader, N for MRT).
  std::vector<int> outPorts;
  for(std::size_t i = 0; i < last->output.size(); ++i)
    if(last->output[i]->type == score::gfx::Types::Image)
      outPorts.push_back(int(i));
  if(outPorts.empty())
  {
    r.error = "final shader has no image output port";
    return r;
  }

  // 3. One offscreen readback sink per output port. Each BackgroundNode owns
  //    its own QRhi/offscreen target and reads its input back into shared_readback.
  std::vector<std::unique_ptr<score::gfx::BackgroundNode>> sinks;
  sinks.reserve(outPorts.size());
  for(std::size_t k = 0; k < outPorts.size(); ++k)
  {
    auto bg = std::make_unique<score::gfx::BackgroundNode>(deviceMode);
    // BackgroundNode's constructor does NOT allocate shared_readback; its
    // renderer dereferences it (*shared_readback) to get the QRhiReadbackResult
    // to fill. A null here => a null QRhiReadbackResult* handed to
    // readBackTexture and a crash in QRhi. The real consumer (RhiPreviewWidget)
    // sets this too — mirror it.
    bg->shared_readback = std::make_shared<QRhiReadbackResult>();
    bg->setSize(size);
    sinks.push_back(std::move(bg));
  }

  // 4. Assemble the graph, render, read back. The Graph is scoped so its
  //    destructor (which calls destroyOutput on the sinks) runs BEFORE the
  //    node unique_ptrs are released at function exit — the graph references
  //    the nodes during teardown.
  {
    score::gfx::Graph graph;

    int32_t nextId = 1;
    for(auto& n : isf)
    {
      n->nodeId = nextId++;
      graph.addNode(n.get());
    }
    for(auto& b : sinks)
    {
      b->nodeId = nextId++;
      graph.addNode(b.get());
    }

    // Chain edges: producer.out[0] -> consumer.first-image-input.
    for(std::size_t i = 0; i + 1 < isf.size(); ++i)
    {
      const int inPort = first_image_input(*isf[i + 1]);
      if(inPort < 0)
      {
        r.error = "chain shader '" + paths[i + 1].toStdString()
                  + "' has no image input to receive the upstream texture";
        // graph destructor + unique_ptr cleanup run on scope exit / return.
        return r;
      }
      graph.addEdge(
          isf[i]->output[0], isf[i + 1]->input[inPort],
          Process::CableType::ImmediateGlutton);
    }

    // Final node's outputs -> the offscreen sinks.
    for(std::size_t k = 0; k < outPorts.size(); ++k)
      graph.addEdge(
          last->output[outPorts[k]], sinks[k]->input[0],
          Process::CableType::ImmediateGlutton);

    GFX_TR("before createAllRenderLists");
    graph.createAllRenderLists(api);
    GFX_TR("after createAllRenderLists");

    // If BackgroundNode couldn't bring up its QRhi (headless, no device), it
    // leaves renderState() null. Treat as a clean skip rather than a failure.
    bool all_ready = true;
    for(auto& b : sinks)
      if(!b->renderState())
        all_ready = false;

    if(!all_ready)
    {
      r.skipped = true;
      r.skip_reason
          = "BackgroundNode could not create a QRhi offscreen context headless "
            "(probe succeeded but full offscreen target allocation failed)";
      return r;
    }

    if(auto rs = sinks[0]->renderState(); rs && rs->rhi)
      r.backend = rs->rhi->backendName();

    // Pump a few frames. Frame 0 builds pipelines/passes; later frames are
    // steady state. The shaders here are time-independent, so the last frame
    // is deterministic.
    for(int f = 0; f < frames; ++f)
    {
      score::gfx::Timings tk{};
      tk.date = ossia::time_value{int64_t(f)};
      tk.parent_duration = ossia::time_value{int64_t(frames)};

      for(auto& n : isf)
      {
        score::gfx::Message m;
        m.node_id = n->nodeId;
        m.token = tk;
        n->process(std::move(m));
      }
      for(auto& b : sinks)
      {
        GFX_TR("render frame");
        b->render();
      }
    }

    GFX_TR("frames done, harvesting");
    // Harvest the readbacks.
    for(std::size_t k = 0; k < sinks.size(); ++k)
    {
      const auto& rb = *sinks[k]->shared_readback;
      ReadbackImage img;
      img.width = rb.pixelSize.width();
      img.height = rb.pixelSize.height();
      img.bytes = rb.data;
      if(!img.valid())
      {
        r.error = "readback of output " + std::to_string(k)
                  + " was empty/short (got " + std::to_string(rb.data.size())
                  + " bytes for " + std::to_string(img.width) + "x"
                  + std::to_string(img.height) + ")";
      }
      r.outputs.push_back(std::move(img));
    }
    GFX_TR("before graph teardown");
  } // graph destroyed here; sinks/isf destroyed at function return.

  GFX_TR("after graph teardown");
#undef GFX_TR
  return r;
}

// -----------------------------------------------------------------------------
// CSF (compute) IMAGE-output readback: convenience over render_isf_chain.
//
//   render_csf_image(be, "csf-image-r32f.cs")
//     single compute node, no input; its write image output is read back.
//
//   render_csf_image(be, "csf-texture-sampling.cs", {"isf-solid-color.fs"})
//     `upstream` is any linear chain whose final image output lands on the
//     CSF's first Types::Image input.
//
// For a CSF that a DOWNSTREAM node must sample, call render_isf_chain directly
// with the CSF first and the sampler last; it builds mixed ISF/CSF chains
// transparently, dispatching on the .cs extension.
// -----------------------------------------------------------------------------
inline IsfResult render_csf_image(
    score::gfx::GraphicsApi backend, const QString& csfPath,
    std::vector<QString> upstream = {}, QSize size = {64, 64}, int frames = 3)
{
  std::vector<QString> chain = std::move(upstream);
  chain.push_back(csfPath);
  return render_isf_chain(backend, std::move(chain), size, frames);
}

// -----------------------------------------------------------------------------
// CSF GEOMETRY / STORAGE-BUFFER readback is not feasible in this fixture.
//
// A geometry-producing CSF writes into a vertex/storage SSBO exposed through a
// Types::Geometry / Types::Buffer output rather than a texture. RenderedCSFNode
// does expose the QRhiBuffer*, and QRhi can read a non-uniform buffer back, so
// this is possible in principle -- but the compute pass only dispatches when the
// node is reachable from an OutputNode sink, and the only headless-constructible
// sink here, BackgroundNode, consumes a Types::Image and reads back a texture.
// A geometry-only CSF is therefore never placed in a RenderList.
//
// Covering it needs a geometry/buffer-consuming sink that also enqueues a
// readBackBuffer. Until then these tests SKIP with this reason.
// -----------------------------------------------------------------------------
inline const char* csf_geometry_readback_skip_reason() noexcept
{
  return "CSF geometry/storage-buffer readback needs a Geometry/Buffer-consuming "
         "sink to drive the compute dispatch and read the produced SSBO back; the "
         "headless fixture only has BackgroundNode (Image-in, texture readback). "
         "Deferred with the raw-raster/VSA sink extension (see Gfx.hpp note).";
}

// =============================================================================
// GfxPipeline -- offscreen render driver for topologies render_isf_chain()
// cannot express.
// =============================================================================
//
// * FAN-OUT: one producer output feeding N BackgroundNode sinks. Each sink is
//   its own OutputNode, hence its own RenderList, renderer instance and
//   persistent storage.
//
// * DIAMOND: one producer output wired to two input ports of one consumer. This
//   gives a single renderer instance two outgoing edges inside one RenderList,
//   the only topology that exercises per-edge, per-instance behaviour such as
//   the ISF persistent-SSBO swap. Two separate sinks do not: those are
//   independent RenderLists with independent storage.
//
// * MUTATION: render, remove an edge through the same incremental path the app
//   uses (GfxContext::incrementalEdgeUpdate -> Graph::onEdgeRemoved +
//   removeEdge + reconcile), render more.
//
// Construct inside run_in_gui_app and never move or return it across the lambda
// boundary; harvest ReadbackImages inside and run Catch2 macros afterwards.
//
// Member order matters: m_graph is declared LAST so it is destroyed FIRST, while
// the sinks and nodes its destructor touches are still alive.
//
// Typical use:
//   GfxPipeline p;
//   const int prod = p.addIsf(corpus("isf-persistent-counter.fs"));
//   const int mix  = p.addIsf(corpus("isf-mix-two.fs"));
//   const int sink = p.addSink({64,64});
//   p.wire(p.imageOut(prod,0), p.imageIn(mix,0));
//   p.wire(p.imageOut(prod,0), p.imageIn(mix,1));
//   p.wire(p.imageOut(mix,0),  p.sinkInput(sink));
//   if(!p.create(backend)) { /* SKIP: p.skipped()/p.skipReason() */ }
//   REQUIRE(p.error().empty());
//   p.render(4);
//   const auto img = p.readback(sink);
// -----------------------------------------------------------------------------
class GfxPipeline
{
public:
  GfxPipeline() = default;
  GfxPipeline(const GfxPipeline&) = delete;
  GfxPipeline& operator=(const GfxPipeline&) = delete;

  /// Build an ISF node from a corpus shader path and register it in the graph.
  /// Returns its node index (for imageIn/imageOut/isf), or -1 on a build error
  /// (see error()). Nodes keep the order they were added.
  int addIsf(const QString& path)
  {
    auto built = make_isf_node(path);
    if(!built.node)
    {
      if(m_error.empty())
        m_error = built.error;
      return -1;
    }
    built.node->nodeId = m_nextId++;
    m_graph.addNode(built.node.get());
    m_isf.push_back(std::move(built.node));
    return int(m_isf.size()) - 1;
  }

  /// Build a CSF compute node and register it in the same index space as ISF,
  /// raster and VSA nodes. This makes buffer, image and geometry producers
  /// available to live-edit fixtures instead of leaving those cable operations
  /// as null-port no-ops.
  int addCsf(const QString& path)
  {
    auto built = make_csf_node(path);
    if(!built.node)
    {
      if(m_error.empty())
        m_error = built.error;
      return -1;
    }
    built.node->nodeId = m_nextId++;
    m_graph.addNode(built.node.get());
    m_isf.push_back(std::move(built.node));
    return int(m_isf.size()) - 1;
  }

  /// Build a RAW_RASTER_PIPELINE node from a .vs + .fs pair and register it.
  /// Returns its node index (indexes the same space as addIsf), or -1 on error.
  int addRaster(const QString& vsPath, const QString& fsPath)
  {
    auto built = make_raster_node(vsPath, fsPath);
    if(!built.node)
    {
      if(m_error.empty())
        m_error = built.error;
      return -1;
    }
    built.node->nodeId = m_nextId++;
    m_graph.addNode(built.node.get());
    m_isf.push_back(std::move(built.node));
    return int(m_isf.size()) - 1;
  }

  /// Build a VSA (vertex-shader-art) node from a .vs and register it.
  /// Returns its node index, or -1 on error.
  int addVsa(const QString& vsPath)
  {
    auto built = make_vsa_node(vsPath);
    if(!built.node)
    {
      if(m_error.empty())
        m_error = built.error;
      return -1;
    }
    built.node->nodeId = m_nextId++;
    m_graph.addNode(built.node.get());
    m_isf.push_back(std::move(built.node));
    return int(m_isf.size()) - 1;
  }

  /// Register a node this fixture did not build — a score::gfx engine node
  /// (ScenePreprocessorNode) or a Crousti-wrapped halp process
  /// (oscr::GfxNode<Threedim::Camera> and friends). Returns an index into a
  /// SEPARATE space from addIsf/addRaster/addVsa: address its ports with
  /// nodePort*/`node()`, not with imageOut/geometryIn.
  int addNode(std::unique_ptr<score::gfx::Node> n)
  {
    if(!n)
    {
      if(m_error.empty())
        m_error = "GfxPipeline::addNode: null node";
      return -1;
    }
    n->nodeId = m_nextId++;
    m_graph.addNode(n.get());
    m_nodes.push_back(std::move(n));
    return int(m_nodes.size()) - 1;
  }

  score::gfx::Node* node(int i) { return m_nodes.at(i).get(); }

  score::gfx::Port* nodeImageOut(int i, int k = 0)
  {
    return nth_image_output(*m_nodes.at(i), k);
  }
  score::gfx::Port* nodeGeometryIn(int i, int k = 0)
  {
    return nth_geometry_input(*m_nodes.at(i), k);
  }
  score::gfx::Port* nodeGeometryOut(int i, int k = 0)
  {
    return nth_geometry_output(*m_nodes.at(i), k);
  }
  score::gfx::Port* nodeBufferOut(int i, int k = 0)
  {
    return nth_buffer_output(*m_nodes.at(i), k);
  }
  /// Scene ports are their own Types:: value on engine nodes
  /// (ScenePreprocessorNode input 0) but halp scene outlets are stamped
  /// Types::Geometry by port_to_type_enum, so a scene edge can be either.
  /// This accepts both so callers wire by intent rather than by enum.
  score::gfx::Port* nodeSceneIn(int i, int k = 0)
  {
    int seen = 0;
    for(auto* p : m_nodes.at(i)->input)
      if(p->type == score::gfx::Types::Scene || p->type == score::gfx::Types::Geometry)
        if(seen++ == k)
          return p;
    return nullptr;
  }
  score::gfx::Port* nodeSceneOut(int i, int k = 0)
  {
    int seen = 0;
    for(auto* p : m_nodes.at(i)->output)
      if(p->type == score::gfx::Types::Scene || p->type == score::gfx::Types::Geometry)
        if(seen++ == k)
          return p;
    return nullptr;
  }

  /// Add an offscreen readback sink of `size`. Returns its sink index.
  int addSink(QSize size = {64, 64})
  {
    auto bg = std::make_unique<score::gfx::BackgroundNode>();
    // Mirror RhiPreviewWidget: the renderer dereferences *shared_readback.
    bg->shared_readback = std::make_shared<QRhiReadbackResult>();
    bg->setSize(size);
    bg->nodeId = m_nextId++;
    m_graph.addNode(bg.get());
    m_sinks.push_back(std::move(bg));
    return int(m_sinks.size()) - 1;
  }

  /// Wire an image edge source-port -> sink-port. Both ports must belong to nodes
  /// already added; nullptr records an error rather than crashing.
  ///
  /// A feedback edge (a node reading its own previous output) MUST pass a Delayed
  /// type: Graph.cpp's no_delay_edges filter keeps only the Immediate kinds before
  /// the topological sort, so an Immediate self-edge makes the graph cyclic.
  void wire(
      score::gfx::Port* source, score::gfx::Port* sink,
      Process::CableType type = Process::CableType::ImmediateGlutton)
  {
    if(!source || !sink)
    {
      if(m_error.empty())
        m_error = "GfxPipeline::wire: null port";
      return;
    }
    m_graph.addEdge(source, sink, type);
  }

  /// Wire a node's output back onto one of its own inputs, the way the GUI's
  /// delayed-cable property does.
  void wireFeedback(score::gfx::Port* source, score::gfx::Port* sink)
  {
    wire(source, sink, Process::CableType::DelayedGlutton);
  }

  /// k-th image output port of ISF node `nodeIdx`.
  score::gfx::Port* imageOut(int nodeIdx, int k = 0)
  {
    return nth_image_output(*m_isf.at(nodeIdx), k);
  }
  /// k-th image input port of ISF node `nodeIdx`.
  score::gfx::Port* imageIn(int nodeIdx, int k = 0)
  {
    return nth_image_input(*m_isf.at(nodeIdx), k);
  }
  /// k-th geometry input port of node `nodeIdx` (raw-raster geometry sink).
  score::gfx::Port* geometryIn(int nodeIdx, int k = 0)
  {
    return nth_geometry_input(*m_isf.at(nodeIdx), k);
  }
  /// k-th geometry output port of node `nodeIdx` (CSF geometry producer).
  score::gfx::Port* geometryOut(int nodeIdx, int k = 0)
  {
    return nth_geometry_output(*m_isf.at(nodeIdx), k);
  }
  /// k-th Buffer input port of node `nodeIdx` (uniform_input / read_only SSBO).
  score::gfx::Port* bufferIn(int nodeIdx, int k = 0)
  {
    return nth_buffer_input(*m_isf.at(nodeIdx), k);
  }
  /// k-th Buffer output port of node `nodeIdx` (CSF storage producer).
  score::gfx::Port* bufferOut(int nodeIdx, int k = 0)
  {
    return nth_buffer_output(*m_isf.at(nodeIdx), k);
  }
  /// The (single) input port of sink `sinkIdx`.
  score::gfx::Port* sinkInput(int sinkIdx) { return m_sinks.at(sinkIdx)->input[0]; }

  /// Bring up all render lists on `api`, probing the backend first. Returns false
  /// with skipped()=true when the backend cannot initialize here or the offscreen
  /// targets cannot be allocated headless; false with error() non-empty on a
  /// genuine build error. On success records the actual backend name.
  /// Let create() proceed on GraphicsApi::Null. Only for a case whose
  /// assertions are STRUCTURAL — which shim/pass/cap was selected — never for
  /// one that reads a pixel: the readback stays empty and readback().valid()
  /// stays false. See null_backend_skip_reason().
  void allowNullBackend(bool v = true) { m_allowNull = v; }

  bool create(score::gfx::GraphicsApi api)
  {
    m_backend = backend_name(api);
    if(!m_error.empty())
      return false;

    if(api == score::gfx::Null && !m_allowNull)
    {
      m_skipped = true;
      m_skipReason = null_backend_skip_reason();
      return false;
    }

    std::string probed;
    if(!probe_api(api, probed))
    {
      m_skipped = true;
      m_skipReason = std::string("RHI backend '") + backend_name(api)
                     + "' cannot initialize on this machine (no driver/ICD, "
                       "wrong platform, or a legacy/unsupported GL context)";
      return false;
    }

    m_graph.createAllRenderLists(api);

    bool all_ready = true;
    for(auto& b : m_sinks)
      if(!b->renderState())
        all_ready = false;
    if(!all_ready)
    {
      m_skipped = true;
      m_skipReason
          = "BackgroundNode could not create a QRhi offscreen context headless "
            "(probe succeeded but full offscreen target allocation failed)";
      return false;
    }

    if(!m_sinks.empty())
      if(auto rs = m_sinks[0]->renderState(); rs && rs->rhi)
        m_backend = rs->rhi->backendName();
    return true;
  }

  /// Pump `frames` frames (each: deliver a token to every ISF node, then render
  /// every sink). Frame indices continue across successive calls, so
  /// render(2); render(2); is a 4-frame run with monotone frame indices — the
  /// mid-run mutation tests rely on that (pre-frames, mutate, post-frames).
  void render(int frames)
  {
    std::vector<score::gfx::Node*> procs;
    procs.reserve(m_isf.size() + m_nodes.size());
    for(auto& n : m_isf)
      procs.push_back(n.get());
    // Crousti-wrapped halp nodes need the per-frame Message too: it is what
    // CustomGfxNodeBase::process stores as last_message, and processControlIn
    // reads it every frame to apply control values onto the halp struct.
    for(auto& n : m_nodes)
      procs.push_back(n.get());
    std::vector<score::gfx::OutputNode*> sinks;
    sinks.reserve(m_sinks.size());
    for(auto& s : m_sinks)
      sinks.push_back(s.get());

    for(int f = 0; f < frames; ++f)
    {
      // parent_duration is only advisory (the shaders under test are
      // frame-driven, not time-driven); pass a stable positive value.
      pump_frame(procs, sinks, int(m_frame), frames);
      ++m_frame;
    }
  }

  /// Remove an image edge through the same incremental path GfxContext uses:
  /// notify the renderers via Graph::onEdgeRemoved BEFORE deleting the edge,
  /// delete it, then reconcile the render lists and refresh passes and samplers.
  /// Must run between render() calls, not rebuild the graph from scratch.
  void removeEdgeIncremental(score::gfx::Port* source, score::gfx::Port* sink)
  {
    if(!source || !sink)
      return;
    score::gfx::Edge* edge = nullptr;
    for(auto* e : source->edges)
      if(e->sink == sink)
      {
        edge = e;
        break;
      }
    if(!edge)
      return;

    m_graph.onEdgeRemoved(*edge, nullptr);
    m_graph.removeEdge(source, sink);
    m_graph.reconcileAllRenderLists();
    m_graph.createAllMissingPasses();
    m_graph.updateAllSinkSamplers();
  }

  /// Remove a node (and every edge touching it) through the SAME path the app
  /// uses on a node deletion (GfxContext::remove_node -> Graph::removeNodeAndEdges).
  /// removeNodeAndEdges notifies the affected render lists, deletes the edges,
  /// releases the node's own renderers AND reconciles every render list — the
  /// reconcile is what releases renderers of nodes that the removal made
  /// transitively unreachable and erases their node->renderedNodes entries (the
  /// leak/UAF regression guard). The node object itself stays alive (owned by the
  /// GfxPipeline); only its graph membership + GPU renderers are torn down. Run it
  /// BETWEEN render() calls.
  void removeNodeIncremental(int nodeIdx)
  {
    m_graph.removeNodeAndEdges(m_isf.at(nodeIdx).get());
    m_graph.removeNode(m_isf.at(nodeIdx).get());
  }

  /// Add an image edge through the SAME incremental path GfxContext uses for a
  /// newly-connected cable (GfxContext::incrementalEdgeUpdate add branch):
  /// addEdge, then reconcile the render lists (which gives a renderer + passes
  /// to any node that just became reachable — e.g. a mid-render-added node) and
  /// refresh passes/samplers. Run it BETWEEN render() calls; the added node/edge
  /// must already be in the graph (addIsf registers the node even after
  /// create()). This is the counterpart of removeEdgeIncremental and exercises
  /// the incremental graph-edit path rather than a from-scratch rebuild.
  void addEdgeIncremental(score::gfx::Port* source, score::gfx::Port* sink)
  {
    if(!source || !sink)
    {
      if(m_error.empty())
        m_error = "GfxPipeline::addEdgeIncremental: null port";
      return;
    }
    m_graph.addEdge(source, sink, Process::CableType::ImmediateGlutton);
    m_graph.reconcileAllRenderLists();
    m_graph.createAllMissingPasses();
    m_graph.updateAllSinkSamplers();
  }

  /// Resize a sink's offscreen target mid-render, driving the SAME path a live
  /// viewport resize takes: BackgroundNode::setSize -> resize() -> the
  /// onResize callback the Graph registered -> Graph::recreateOutputRenderList
  /// (drains the GPU, tears down + rebuilds that output's RenderList at the new
  /// size). Run between render() calls; the next render()'s readback comes back
  /// at `newSize`. Targets the resize P0 class (rebuild-under-render must not
  /// crash and must produce correct pixels at the new dimensions).
  void resizeSink(int sinkIdx, QSize newSize)
  {
    m_sinks.at(sinkIdx)->setSize(newSize);
  }

  /// Read back sink `sinkIdx`'s current offscreen texture as RGBA8.
  ReadbackImage readback(int sinkIdx) const
  {
    ReadbackImage img;
    const auto& rb = *m_sinks.at(sinkIdx)->shared_readback;
    img.width = rb.pixelSize.width();
    img.height = rb.pixelSize.height();
    img.bytes = rb.data;
    return img;
  }

  score::gfx::ISFNode* isf(int i) { return m_isf.at(i).get(); }
  score::gfx::BackgroundNode* sink(int i) { return m_sinks.at(i).get(); }
  score::gfx::Graph& graph() { return m_graph; }

  bool skipped() const { return m_skipped; }
  const std::string& skipReason() const { return m_skipReason; }
  const std::string& error() const { return m_error; }
  const std::string& backend() const { return m_backend; }

private:
  int32_t m_nextId = 1;
  int64_t m_frame = 0;
  bool m_allowNull = false;
  bool m_skipped = false;
  std::string m_skipReason;
  std::string m_error;
  std::string m_backend;

  // Declaration order == construction order; destruction is the reverse, so
  // m_graph (last) is torn down FIRST while the sinks and nodes it references
  // are still alive. DO NOT reorder.
  std::vector<std::unique_ptr<score::gfx::ISFNode>> m_isf;
  std::vector<std::unique_ptr<score::gfx::Node>> m_nodes;
  std::vector<std::unique_ptr<score::gfx::BackgroundNode>> m_sinks;
  score::gfx::Graph m_graph;
};

// -----------------------------------------------------------------------------
// FAN-OUT convenience: render a linear ISF chain, then attach `nSinks`
// BackgroundNode sinks to the SAME image output port (`outputPort`) of the
// terminal node. Returns one ReadbackImage per sink in IsfResult::outputs
// (outputs[k] is sink k), so a test can assert all sinks agree.
//
// NOTE: because each BackgroundNode is its own OutputNode / RenderList, the
// sinks render independently. This is exactly the shape the dangling-sink
// sampler regression needs (A -> {B, C}, remove A->B, C survives). It is NOT
// sufficient on its own to reproduce the persistent double-swap — see the
// DIAMOND note on GfxPipeline for why that needs >=2 edges on one instance.
// -----------------------------------------------------------------------------
inline IsfResult render_isf_fanout(
    score::gfx::GraphicsApi backend, std::vector<QString> paths, int nSinks,
    int outputPort = 0, QSize size = {64, 64}, int frames = 3)
{
  IsfResult r;
  r.backend = backend_name(backend);
  if(paths.empty() || nSinks < 1)
  {
    r.error = "render_isf_fanout: need >=1 shader path and >=1 sink";
    return r;
  }

  GfxPipeline p;
  std::vector<int> nodeIdx;
  for(const auto& path : paths)
  {
    const int idx = p.addIsf(path);
    if(idx < 0)
    {
      r.error = p.error();
      return r;
    }
    nodeIdx.push_back(idx);
  }

  // Linear chain: node[i].out0 -> node[i+1].image-in-0.
  for(std::size_t i = 0; i + 1 < nodeIdx.size(); ++i)
  {
    auto* out = p.imageOut(nodeIdx[i], 0);
    auto* in = p.imageIn(nodeIdx[i + 1], 0);
    if(!in)
    {
      r.error = "render_isf_fanout: chain shader has no image input to receive "
                "the upstream texture";
      return r;
    }
    p.wire(out, in);
  }

  // Fan the terminal node's chosen output port out to nSinks sinks.
  const int terminal = nodeIdx.back();
  auto* termOut = p.imageOut(terminal, outputPort);
  if(!termOut)
  {
    r.error = "render_isf_fanout: terminal shader has no image output at the "
              "requested port index";
    return r;
  }
  std::vector<int> sinks;
  for(int k = 0; k < nSinks; ++k)
  {
    const int s = p.addSink(size);
    p.wire(termOut, p.sinkInput(s));
    sinks.push_back(s);
  }

  if(!p.create(backend))
  {
    r.backend = p.backend();
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = p.error();
    return r;
  }
  r.backend = p.backend();

  p.render(frames);

  for(int s : sinks)
  {
    ReadbackImage img = p.readback(s);
    if(!img.valid())
      r.error = "render_isf_fanout: sink readback empty/short ("
                + std::to_string(img.bytes.size()) + " bytes)";
    r.outputs.push_back(std::move(img));
  }
  return r;
}

// -----------------------------------------------------------------------------
// CONTROL-VALUE render: build a SINGLE ISF node, inject control values on it,
// render it into one offscreen sink and read the (single) image output back.
//
//   `controls` is a list of {inputPortIndex, ossia::value}. Each is applied via
//   setControl() AFTER the render list is up and BEFORE the frames are pumped,
//   so the shader sees the injected values (not just the ISF DEFAULTs). This is
//   the primary vehicle for the control-inlet ISF cases
//   (float / color / point2D / point3D / bool / long): pick a shader that emits
//   its control verbatim and the readback becomes analytic (e.g. inject
//   col=(0.2,0.4,0.6,1) into isf-control-color.fs -> every pixel reads back
//   round(255*col), the target being a plain non-sRGB RGBA8).
//
//   render_isf_controls(be, corpus("isf-control-color.fs"),
//                       {{0, ossia::vec4f{0.2f,0.4f,0.6f,1.f}}})
// -----------------------------------------------------------------------------
struct ControlSetting
{
  int port;          // raw index into node->input (see setControl doc)
  ossia::value value;
};

inline IsfResult render_isf_controls(
    score::gfx::GraphicsApi backend, const QString& path,
    std::vector<ControlSetting> controls, QSize size = {64, 64}, int frames = 3)
{
  IsfResult r;
  r.backend = backend_name(backend);

  GfxPipeline p;
  const int n = p.addIsf(path);
  if(n < 0)
  {
    r.error = p.error();
    return r;
  }
  auto* out = p.imageOut(n, 0);
  if(!out)
  {
    r.error = "control shader '" + path.toStdString()
              + "' has no image output port";
    return r;
  }
  const int s = p.addSink(size);
  p.wire(out, p.sinkInput(s));

  if(!p.create(backend))
  {
    r.backend = p.backend();
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = p.error();
    return r;
  }
  r.backend = p.backend();

  // Inject after the render list exists, before the frames are pumped.
  for(const auto& c : controls)
    setControl(*p.isf(n), c.port, c.value);

  p.render(frames);

  ReadbackImage img = p.readback(s);
  if(!img.valid())
    r.error = "control render sink readback empty/short ("
              + std::to_string(img.bytes.size()) + " bytes)";
  r.outputs.push_back(std::move(img));
  return r;
}

// -----------------------------------------------------------------------------
// AUDIO render: build a SINGLE ISF node, feed a known audio buffer into its
// TYPE:"audio" inlet (port `audioPort`), render into one sink and read the image
// output back. `controls` optionally sets extra control inlets first.
// -----------------------------------------------------------------------------
inline IsfResult render_isf_audio(
    score::gfx::GraphicsApi backend, const QString& path, int audioPort,
    const ossia::audio_vector& audio, std::vector<ControlSetting> controls = {},
    QSize size = {64, 64}, int frames = 4)
{
  IsfResult r;
  r.backend = backend_name(backend);

  GfxPipeline p;
  const int n = p.addIsf(path);
  if(n < 0)
  {
    r.error = p.error();
    return r;
  }
  auto* out = p.imageOut(n, 0);
  if(!out)
  {
    r.error = "audio shader '" + path.toStdString() + "' has no image output port";
    return r;
  }
  const int s = p.addSink(size);
  p.wire(out, p.sinkInput(s));

  if(!p.create(backend))
  {
    r.backend = p.backend();
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = p.error();
    return r;
  }
  r.backend = p.backend();

  for(const auto& c : controls)
    setControl(*p.isf(n), c.port, c.value);
  setAudio(*p.isf(n), audioPort, audio);

  p.render(frames);

  ReadbackImage img = p.readback(s);
  if(!img.valid())
    r.error = "audio render sink readback empty/short ("
              + std::to_string(img.bytes.size()) + " bytes)";
  r.outputs.push_back(std::move(img));
  return r;
}

// -----------------------------------------------------------------------------
// RAW-RASTER (RenderPipeline) render + readback.
//
//   render_raster(be, "csf-vertex-count-expr.cs", "raw-raster-basic.vs",
//                 "raw-raster-basic.fs")
//
// Builds the cross-node pipeline
//   CSF geometry producer  --Geometry-->  raw-raster node  --Image-->  sink(s)
// and reads back each of the raster node's color attachments (one sink per
// FRAGMENT_OUTPUTS entry, so MRT shaders return N outputs). The CSF compute
// pass is dispatched because the raw-raster node pulls on its Geometry output,
// which is exactly the geometry-consuming path that (a) exercises RAW_RASTER and
// (b) validates the produced geometry INDIRECTLY through the drawn pixels.
//
// `geoStages` may be:
//   * empty        — PROCEDURAL raw-raster shader (no VERTEX_INPUTS,
//                    PIPELINE_STATE.VERTEX_COUNT set): no geometry node/edge is
//                    created and the raster node draws gl_VertexIndex vertices.
//   * one path     — a single CSF geometry producer feeds the raster node.
//   * several paths— a CSF geometry CHAIN: geoStages[0].geoOut -> geoStages[1]
//                    .geoIn -> ... -> raster.geoIn. Needed for read_write
//                    geometry processors (the stride corpus) that read+modify an
//                    upstream buffer rather than allocate one; a write_only
//                    producer must sit first in the chain to give them input.
// -----------------------------------------------------------------------------
inline IsfResult render_raster(
    score::gfx::GraphicsApi backend, std::vector<QString> geoStages,
    const QString& vsPath, const QString& fsPath, QSize size = {64, 64},
    int frames = 3)
{
  IsfResult r;
  r.backend = backend_name(backend);

  GfxPipeline p;

  std::vector<int> geo;
  for(const auto& g : geoStages)
  {
    const int idx = p.addIsf(g); // .cs => CSF geometry producer/processor
    if(idx < 0)
    {
      r.error = "geometry stage build failed (" + g.toStdString()
                + "): " + p.error();
      return r;
    }
    geo.push_back(idx);
  }

  const int raster = p.addRaster(vsPath, fsPath);
  if(raster < 0)
  {
    r.error = "raw-raster node build failed: " + p.error();
    return r;
  }

  // Geometry edges: chain the producers, then feed the raster node's geo input.
  for(std::size_t i = 0; i + 1 < geo.size(); ++i)
  {
    auto* gout = p.geometryOut(geo[i], 0);
    auto* gin = p.geometryIn(geo[i + 1], 0);
    if(!gout || !gin)
    {
      r.error = "geometry chain stage missing a Geometry port";
      return r;
    }
    p.wire(gout, gin);
  }
  if(!geo.empty())
  {
    auto* gout = p.geometryOut(geo.back(), 0);
    auto* gin = p.geometryIn(raster, 0);
    if(!gout)
    {
      r.error = "geometry producer '" + geoStages.back().toStdString()
                + "' has no Geometry output port";
      return r;
    }
    if(!gin)
    {
      r.error = "raw-raster node has no Geometry input port";
      return r;
    }
    p.wire(gout, gin);
  }

  // One sink per raster image output (MRT => several).
  int nOut = 0;
  for(auto* op : p.isf(raster)->output)
    if(op->type == score::gfx::Types::Image)
      ++nOut;
  if(nOut == 0)
  {
    r.error = "raw-raster node has no image output port";
    return r;
  }

  std::vector<int> sinks;
  for(int k = 0; k < nOut; ++k)
  {
    const int s = p.addSink(size);
    p.wire(p.imageOut(raster, k), p.sinkInput(s));
    sinks.push_back(s);
  }

  if(!p.create(backend))
  {
    r.backend = p.backend();
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = p.error();
    return r;
  }
  r.backend = p.backend();

  p.render(frames);

  for(int s : sinks)
  {
    ReadbackImage img = p.readback(s);
    if(!img.valid())
      r.error = "raw-raster sink readback empty/short ("
                + std::to_string(img.bytes.size()) + " bytes)";
    r.outputs.push_back(std::move(img));
  }
  return r;
}

// -----------------------------------------------------------------------------
// VSA (vertex-shader-art) render + readback. Builds the VSA node from a single
// .vs, renders it offscreen, and reads back its (single) color output.
// -----------------------------------------------------------------------------
inline IsfResult render_vsa(
    score::gfx::GraphicsApi backend, const QString& vsPath, QSize size = {64, 64},
    int frames = 3)
{
  IsfResult r;
  r.backend = backend_name(backend);

  GfxPipeline p;
  const int vsa = p.addVsa(vsPath);
  if(vsa < 0)
  {
    r.error = "VSA node build failed: " + p.error();
    return r;
  }

  auto* out = p.imageOut(vsa, 0);
  if(!out)
  {
    r.error = "VSA node has no image output port";
    return r;
  }
  const int s = p.addSink(size);
  p.wire(out, p.sinkInput(s));

  if(!p.create(backend))
  {
    r.backend = p.backend();
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = p.error();
    return r;
  }
  r.backend = p.backend();

  p.render(frames);

  ReadbackImage img = p.readback(s);
  if(!img.valid())
    r.error = "VSA sink readback empty/short ("
              + std::to_string(img.bytes.size()) + " bytes)";
  r.outputs.push_back(std::move(img));
  return r;
}

} // namespace score::test::gfx
