// Extract buffer (by name) in Buffer mode reads an auxiliary buffer of a
// geometry that has no vertices.
//
// Scene Preprocessor's first geometry for a scene holding only splats is a
// 0-vertex carrier whose auxiliary list names its per-frame buffers (camera,
// lights, ...). Extract buffer (by name) returned early on vertices == 0 in
// both modes, so asking it for `camera` produced a null buffer. Here a halp
// node publishes such a carrier: 0 vertices, one buffer, an auxiliary named
// `camera` over it. The buffer is syn-storage-colour.cs's storage resource
// (green); Extract buffer set to Buffer / "camera" feeds rr-storage-input,
// which paints the colour it reads: green when the buffer arrives, black for
// the unbound placeholder. Attribute mode on the same carrier stays empty.
//
// Registration: see the test_gfx_extract_buffer_aux_carrier_f2 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/ExtractBuffer2.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/buffer.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct AuxOnlyCarrier
{
  halp_meta(name, "Aux-only carrier")
  halp_meta(c_name, "test_aux_only_carrier_f2")
  halp_meta(category, "Test")
  halp_meta(uuid, "0f6f3a52-9d8e-4c1b-b7a2-5e41c93d2f08")

  struct
  {
    halp::gpu_buffer_input<"Buffer"> buffer;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;
  } outputs;

  void operator()()
  {
    const auto& in = inputs.buffer.buffer;
    auto& mesh = outputs.geometry.mesh;
    if(in.handle == m_last)
    {
      outputs.geometry.dirty_mesh = false;
      return;
    }
    m_last = in.handle;
    mesh.buffers.clear();
    mesh.auxiliary.clear();
    mesh.vertices = 0;
    if(in.handle)
    {
      mesh.buffers.push_back(
          halp::geometry_gpu_buffer{
              .handle = in.handle, .byte_size = in.byte_size, .dirty = true});
      mesh.auxiliary.push_back(
          {.name = "camera", .buffer = 0, .byte_offset = 0, .byte_size = in.byte_size});
    }
    outputs.geometry.dirty_mesh = true;
  }

  void* m_last{};
};

QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;
  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

struct Shot
{
  bool skipped{};
  std::string error;
  std::array<uint8_t, 4> centre{};
};

Shot render(score::gfx::GraphicsApi api, int mode)
{
  Shot s;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      s.error = "no document";
      return;
    }
    HalpProcesses procs;
    GfxPipeline p;
    const int producer = p.addCsf(corpus("syn-storage-colour.cs"));
    auto carrierNode = procs.make<AuxOnlyCarrier>(doc->context());
    auto* carrier = carrierNode.get();
    const int ca = p.addNode(std::move(carrierNode));
    auto extractNode = procs.make<Threedim::ExtractBuffer2>(doc->context());
    auto* extract = extractNode.get();
    const int ex = p.addNode(std::move(extractNode));
    const int consumer
        = p.addRaster(corpus("rr-storage-input.vs"), corpus("rr-storage-input.fs"));
    if(producer < 0 || ca < 0 || ex < 0 || consumer < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    auto* storageOut = p.bufferOut(producer, 0);
    auto* bufOut = p.nodeBufferOut(ex, 0);
    auto* bufIn = p.bufferIn(consumer, 0);
    if(!storageOut || !bufOut || !bufIn || carrier->input.empty()
       || carrier->output.empty() || extract->input.empty())
    {
      s.error = "ports missing";
      return;
    }
    p.wire(storageOut, carrier->input[0]);
    p.wire(carrier->output[0], extract->input[0]);
    p.wire(bufOut, bufIn);
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));

    score::gfx::Message m;
    m.node_id = extract->nodeId;
    m.input.push_back(ossia::value{});
    m.input.push_back(ossia::value{mode});
    m.input.push_back(ossia::value{std::string{"camera"}});
    m.input.push_back(ossia::value{false});
    extract->process(std::move(m));

    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(6);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      s.error = "empty readback";
      return;
    }
    s.centre = img.at(img.width / 2, img.height / 2);
  });
  return s;
}
}

TEST_CASE(
    "Extract buffer (by name) outputs an auxiliary of a 0-vertex geometry",
    "[gfx][threedim][extractbuffer][f2]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  SECTION("Buffer mode reads the auxiliary")
  {
    const auto s = render(api, int(Threedim::ExtractBuffer2::Buffer));
    if(s.skipped)
      SKIP("backend unavailable");
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);
    INFO("error=" << s.error);
    REQUIRE(s.error.empty());
    INFO(
        "centre=(" << int(s.centre[0]) << "," << int(s.centre[1]) << ","
                   << int(s.centre[2]) << ")");
    CHECK(s.centre[0] < 20);
    CHECK(s.centre[1] > 230);
    CHECK(s.centre[2] < 20);
  }

  SECTION("Attribute mode has nothing to read on a 0-vertex geometry")
  {
    const auto s = render(api, int(Threedim::ExtractBuffer2::Attribute));
    if(s.skipped)
      SKIP("backend unavailable");
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);
    INFO("error=" << s.error);
    REQUIRE(s.error.empty());
    INFO(
        "centre=(" << int(s.centre[0]) << "," << int(s.centre[1]) << ","
                   << int(s.centre[2]) << ")");
    CHECK(s.centre[1] < 20);
  }
}
