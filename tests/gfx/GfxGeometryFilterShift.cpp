// =============================================================================
// P2-9 -- "the Geometry Filter changes geometry on the GPU thread"
// (SPEC-SCENE-RENDER-TESTS.md §3.3, row P2-9: `syn-geofilter-shift.glsl` shifts
//  positions by a known delta; the drawn silhouette moves by exactly that;
//  negative control "zero the delta".)
//
// REGISTRATION (add to tests/gfx/CMakeLists.txt next to the other
// score_add_gfx_test lines; this file needs nothing beyond the default
// score_add_gfx_test surface -- see the WHAT IS *NOT* HERE section for why that
// is both sufficient and limiting):
//
//     score_add_gfx_test(geometry_filter_shift GfxGeometryFilterShift.cpp)
//
// ctest name: test_gfx_geometry_filter_shift
//
// -----------------------------------------------------------------------------
// WHAT A GEOMETRY FILTER ACTUALLY DOES (read from the engine, not assumed)
// -----------------------------------------------------------------------------
// A Geometry Filter does NOT rewrite vertex buffers. GeometryFilterNodeRenderer
// has no compute pass at all: runRenderPass is empty
// (GeometryFilterNodeRenderer.cpp:128-132) and runInitialPasses only forwards
// the upstream mesh list and APPENDS one ossia::geometry_filter descriptor to
// the geometry_spec it hands the consumer:
//
//     GeometryFilterNodeRenderer.cpp:94    outputGeometry.meshes = geometry.meshes;
//     GeometryFilterNodeRenderer.cpp:110-111
//         outputGeometry.filters->filters.push_back(
//             ossia::geometry_filter{this->nodeId, parent.m_index,
//                                    parent.m_shader, 1});
//     GeometryFilterNodeRenderer.cpp:113   rendered_node->second->process(n, this->outputGeometry);
//
// runInitialPasses runs on the render thread inside RenderList::render, so this
// push IS the "on the GPU thread" half of the case: the filter's GLSL and its
// identity cross to the consumer's renderer once per frame, and its delta
// crosses to GPU memory as a Dynamic UniformBuffer uploaded from
// GeometryFilterNodeRenderer::update (GeometryFilterNodeRenderer.cpp:48-53,
// allocated at :34-35).
//
// The DISPLACEMENT itself happens in the CONSUMER's vertex shader. The consumer
// splices the filter's GLSL into its own vertex stage and calls it per vertex:
//
//     ModelDisplayNode.cpp:1157-1176  processVertexShader() -- for each filter
//         in mesh.filters: replace "%next%" with the next UBO binding, append
//         the filter source to %vtx_define_filters%, and emit
//         "process_vertex_<filter_id>(in_position, in_normal, in_uv,
//          in_tangent, in_color);" into %vtx_do_filters% (:1167-1170).
//     ModelDisplayNode.cpp:858-885     initPasses_impl() -- resolve each
//         filter's node_id back to its GeometryFilterNodeRenderer and bind
//         c->material() as a vertex-stage uniform buffer at that binding.
//
// FILTER DISPATCH SITE, for the record: ModelDisplayNode.cpp:1167-1170 is where
// the filter is actually invoked per vertex; ModelDisplayNode.cpp:868-874 is
// where its delta UBO is bound.
//
// -----------------------------------------------------------------------------
// WHAT IS *NOT* HERE, AND WHY (the honest subset -- please read before adding to
// this file)
// -----------------------------------------------------------------------------
// The silhouette half of P2-9 cannot be made green from a `score_add_gfx_test`
// target, because score_plugin_gfx contains NO consumer that applies geometry
// filters. Verified by exhaustive grep over the whole tree:
//
//   * "%vtx_define_filters%" / "%vtx_do_filters%" occur in exactly two files:
//       - Threedim/ModelDisplay/ModelDisplayNode.cpp  (the real consumer, in
//         score-plugin-threedim, which score_add_gfx_test does not link)
//       - Gfx/GeometryFilter/Process.cpp:86,120       (a COMPILE-ONLY probe:
//         Model::validate splices the filter into a throwaway vertex shader and
//         asks ShaderCache to compile it -- it never renders and never binds
//         the material UBO)
//   * RenderedRawRasterPipelineNode (the only geometry consumer reachable here)
//     never reads mesh.filters at all -- the single occurrence of the word in
//     that file is the FIXME at RenderedRawRasterPipelineNode.cpp:2630.
//   * RenderedCSFNode goes the other way and CLEARS them
//     (RenderedCSFNode.cpp:2344  binding.outputGeometry.filters = {};).
//
// This also EXPLAINS a defect the tree currently records as unexplained:
// tests/gfx/CroustiCpuNodes.cpp's "a geometry filter displaces the mesh it is
// given" is [!shouldfail] with a comment saying the failure "is still
// unexplained. Do not assume one fix covers both." It is explained: that test
// wires the filter into a raw-raster consumer (p.addRaster), and the raw raster
// discards mesh.filters. Nothing is wrong with the filter node there.
//
// So this file asserts, closed-form:
//   (1) the exact vertex program the filter publishes  -- CPU, no RHI;
//   (2) that program and the filter's identity arriving at the consumer's
//       renderer during a real offscreen render, i.e. on the GPU thread;
//   (3) the drawing control: with the delta at 0 the chain draws the
//       full-viewport silhouette, exactly;
//   (4) [!shouldfail] the actual P2-9 oracle -- the silhouette displaced by
//       exactly the delta. Red today for reason (WHAT IS NOT HERE) above, not
//       because of anything in GeometryFilterNode.
//
// -----------------------------------------------------------------------------
// NEGATIVE CONTROLS (product-side; each is ONE line and names the assertions it
// must redden)
// -----------------------------------------------------------------------------
// NC-1 -- kills the dispatch. In score-plugin-gfx/Gfx/Graph/
//   GeometryFilterNodeRenderer.cpp:110-111, comment out the
//     outputGeometry.filters->filters.push_back(
//         ossia::geometry_filter{this->nodeId, parent.m_index, parent.m_shader, 1});
//   MUST redden, by name, every CHECK/REQUIRE in
//     TEST_CASE "a geometry filter publishes its vertex program to the consumer
//                on the render thread"
//   (filterCount == 1, filterNodeId, filterId, filterDirty, filterShader).
//   It must NOT touch the silhouette-control test, which does not depend on the
//   filter list at all -- that asymmetry is the point of splitting them.
//
// NC-2 -- "zero the delta", located in real code. In the SAME file,
//   GeometryFilterNodeRenderer.cpp:52, replace
//     res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
//   with a zero-filled upload, e.g.
//     { const std::vector<char> z(m_materialSize, 0);
//       res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, z.data()); }
//   This is the spec's named control. It is honestly NOT demonstrable today:
//   the assertion it targets (the displaced silhouette, TEST_CASE 4) is already
//   red for the structural reason above, so zeroing the delta cannot change its
//   verdict. Record it here so that whoever makes TEST_CASE 4 green -- by
//   giving RenderedRawRasterPipelineNode the ModelDisplayNode.cpp:1157-1176
//   splice, or by re-registering this file against score_plugin_threedim and
//   swapping the raw raster for a ModelDisplayNode -- can immediately run the
//   real control. NC-2 is unverified as a red-maker until then; NC-1 is the
//   control that works today.
//
// NC-3 -- kills the name mangling. In score-plugin-gfx/3rdparty/libisf/src/
//   isf.cpp:3221, drop the per-node suffix:
//     for(auto& func : funcs)
//       boost::algorithm::replace_all(geomWithoutISF, func, func + "_%node%");
//   -> replace_all(geomWithoutISF, func, func);
//   MUST redden the "process_vertex_7(" / "no un-suffixed process_vertex("
//   assertions in TEST_CASE 1. Two filters on one mesh would then define the
//   same GLSL function twice and the consumer's vertex shader would not compile
//   (ModelDisplayNode.cpp:1166 concatenates every filter's source verbatim).
//
// -----------------------------------------------------------------------------
// CLOSED-FORM ORACLES (every number below is derived, none observed)
// -----------------------------------------------------------------------------
// A. THE PUBLISHED VERTEX PROGRAM.  isf::parser::parse_geometry_filter
//    (isf.cpp:3198-3258) transforms corpus/syn-geofilter-shift.glsl thus:
//      - erase the ISF header and trim              (isf.cpp:3205-3212)
//      - "this_filter"     -> "filter_%node%"       (isf.cpp:3222)
//      - every function name gets "_%node%"         (isf.cpp:3219-3224;
//        "process_vertex" is force-inserted into the set at :3219)
//      - prepend a std140 UBO block, one member per INPUT, typed by
//        create_val_visitor_450 (float_input -> "float", isf.cpp:3053):
//            layout(std140, binding = %next%) uniform filter_%node%_t {
//              float shift;
//            } filter_%node%;
//        followed by a blank line                   (isf.cpp:3228-3255)
//      - m_geometry_filter = ubo + body + "\n"      (isf.cpp:3257)
//    then GeometryFilterNode's constructor substitutes the node index for
//    "%node%" (GeometryFilterNode.cpp:183). "%next%" is deliberately LEFT
//    unresolved -- the consumer picks the binding (ModelDisplayNode.cpp:1164-65).
//    With index 7 that is exactly expected_filter_program(7) below. The corpus
//    body is syn-geofilter-shift.glsl:12-15.
//
// B. PORT SURFACE / MATERIAL SIZE.  GeometryFilterNode.cpp:186-187 pushes one
//    Types::Geometry input and one Types::Geometry output; then
//    geometry_input_port_vis::operator()(float_input) (GeometryFilterNode.cpp
//    :18-27) pushes ONE Types::Float port per float INPUT. syn-geofilter-shift
//    .glsl declares exactly one INPUT ("shift", line 8), so input.size() == 2
//    (Geometry at 0, "shift" at 1) and output.size() == 1. m_materialSize is
//    isf_input_size_vis's sum, 4 bytes for one float
//    (ISFVisitors.hpp:153), so the Dynamic UniformBuffer the renderer allocates
//    is 4 bytes (GeometryFilterNodeRenderer.cpp:34-35).
//
// C. THE PUBLISHED IDENTITY.  From the aggregate initialiser at
//    GeometryFilterNodeRenderer.cpp:111 against ossia::geometry_filter's member
//    order (geometry_port.hpp:391-409  {node_id, filter_id, shader,
//    dirty_index}):
//        node_id     == the filter renderer's nodeId, which GfxPipeline::addNode
//                       copies from the Node (Gfx.hpp:1051  n->nodeId = m_nextId++)
//                       and RenderList gives to the renderer;
//        filter_id   == GeometryFilterNode::m_index == 7 here (the same value
//                       baked into the function name, which is what makes
//                       ModelDisplayNode.cpp:1167-1170's call resolve);
//        shader      == GeometryFilterNode::m_shader, byte-identical to (A);
//        dirty_index == 1, the literal at :111.
//    Exactly ONE entry: syn-geo-producer.cs publishes no filters of its own, and
//    the copy loop at :100-108 copies an empty upstream list.
//
// D. THE SILHOUETTE.  corpus/syn-geo-producer.cs:29-35 emits three vertices at
//    NDC (-1,-1), (3,-1), (-1,3) -- the half-plane intersection
//        x >= -1,  y >= -1,  x + y <= 2
//    which contains the whole [-1,1]^2 viewport. corpus/syn-raster-single.fs:17
//    paints (PASSINDEX/255, 1, 0, 1): green channel 255, red 0. The clear is
//    Qt::transparent (RenderedRawRasterPipelineNode.cpp:3148-3151), so
//    "lit" == G > 128 separates drawn from cleared with no tolerance games.
//    A pixel centre in a WxH readback sits at NDC x = -1 + 2*(px + 0.5)/W.
//    After "position.x += s" the left edge becomes x >= -1 + s, so
//        px is lit  <=>  2*(px + 0.5)/W >= s  <=>  px >= s*W/2 - 0.5.
//    The shifted hypotenuse x + y <= 2 + s never bites for s >= 0 (the extreme
//    pixel centre is at x + y = 2 - 2/W < 2), nor do y >= -1 or x >= -1 + s on
//    the right, so every lit column is lit for all H rows. Hence at W = H = 64:
//        s = 0.0  ->  first lit column 0,  last 63, lit pixels 64*64 = 4096
//        s = 0.5  ->  px >= 15.5 -> 16;   first 16, last 63,
//                     lit pixels (63-16+1)*64 = 48*64 = 3072
//    0.0f and 0.5f are exactly representable, and 15.5 is not an integer, so the
//    boundary is not a tie. The readback's X axis is unflipped on every backend
//    (GfxOrientationMatrix.cpp; only Y is corrected), so column 16 is column 16
//    everywhere.
//
// -----------------------------------------------------------------------------
// A TRAP, DOCUMENTED SO NOBODY RE-DISCOVERS IT
// -----------------------------------------------------------------------------
// A freshly built GeometryFilterNode does NOT start at the shader's declared
// DEFAULT. GeometryFilterNode.cpp:20-23 uses the DEFAULT only when it is
// non-zero and otherwise substitutes (MAX - MIN)/2. syn-geofilter-shift.glsl:8
// declares DEFAULT 0.0, MIN -10, MAX 10, so the initial delta in the material
// buffer is 10.0f, not 0.0f. (ISFNode.cpp:23-26 does the identical thing, so
// this is a house-wide convention rather than a bug in the filter node -- it is
// not asserted here either way.) Every leg below therefore sets the control
// EXPLICITLY before create(); do not assume an unset filter is a no-op filter.
//
// SKIP semantics follow the rest of tests/gfx/ exactly: GENERATE over
// platform_backends(), and `if(r.skipped) SKIP(backend + ": " + reason)` when
// GfxPipeline::create could not bring up a QRhi headless (Gfx.hpp:1180-1203).
// No Catch2 macro runs inside run_in_gui_app -- results are collected into a
// plain struct first, per the fixture header's rule.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/GeometryFilterNode.hpp>
#include <Gfx/Graph/GeometryFilterNodeRenderer.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QFile>

#include <cstring>
#include <limits>
#include <string>

using namespace score::test;
using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
//! The node index handed to GeometryFilterNode. Deliberately neither 0 nor 1 so
//! a hard-coded index in the name mangling (isf.cpp:3221) or in the published
//! descriptor (GeometryFilterNodeRenderer.cpp:111) cannot pass by accident.
constexpr int kFilterIndex = 7;

//! 64x64 keeps oracle (D) integral: 0.5 * 64 / 2 = 16 exactly.
const QSize kSize{64, 64};
constexpr int kFrames = 4;

//! Oracle (A): the exact program GeometryFilterNode publishes for
//! corpus/syn-geofilter-shift.glsl at node index `index`. Derived from
//! isf.cpp:3198-3258 + GeometryFilterNode.cpp:183; NOT copied from a run.
std::string expected_filter_program(int index)
{
  const std::string n = std::to_string(index);
  return "layout(std140, binding = %next%) uniform filter_" + n + "_t {\n"
         "  float shift;\n"
         "} filter_"
         + n
         + ";\n"
           "\n"
           "void process_vertex_"
         + n
         + "(inout vec3 position, inout vec3 normal, inout vec2 uv, inout vec3 "
           "tangent, inout vec4 color)\n"
           "{\n"
           "  position.x += filter_"
         + n
         + ".shift;\n"
           "}\n";
}

//! Build a GeometryFilterNode from a MODE:"GEOMETRY_FILTER" source on disk.
//! Mirrors tests/gfx/CroustiCpuNodes.cpp:545-568, with the node index exposed
//! so the mangling can be asserted against a non-trivial value.
std::unique_ptr<score::gfx::GeometryFilterNode>
make_geometry_filter(const QString& path, int index, std::string& err)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    err = "cannot open geometry filter: " + path.toStdString();
    return {};
  }
  const auto src = QString::fromUtf8(f.readAll()).toStdString();
  try
  {
    ::isf::parser p{src, ::isf::parser::ShaderType::GeometryFilter};
    auto desc = p.data();
    return std::make_unique<score::gfx::GeometryFilterNode>(
        int64_t(index), desc, QString::fromStdString(p.geometry_filter()));
  }
  catch(const std::exception& e)
  {
    err = std::string("geometry filter parse failed: ") + e.what();
    return {};
  }
  catch(...)
  {
    err = "geometry filter parse failed: unknown error";
    return {};
  }
}

//! Everything one run of the chain observes. Never thrown from inside
//! run_in_gui_app -- Catch2 macros run on this AFTER it returns.
struct FilterRun
{
  bool built = false;
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  // --- the filter node itself (oracle B)
  std::size_t inputPorts = 0;
  std::size_t outputPorts = 0;
  int materialSize = -1;
  float materialShift = std::numeric_limits<float>::quiet_NaN();
  bool uboPresent = false;
  int uboSize = -1;
  int64_t filterOwnNodeId = -2;

  // --- what crossed to the consumer on the render thread (oracle C)
  bool consumerSawGeometry = false;
  bool consumerSawFilterList = false;
  std::size_t filterCount = 0;
  int64_t filterNodeId = -1;
  int64_t filterId = -1;
  int64_t filterDirty = -1;
  std::string filterShader;

  // --- what was drawn (oracle D)
  int width = 0;
  int height = 0;
  long lit = 0;
  int firstLitColumn = -1;
  int lastLitColumn = -1;
  //! lit pixels that are NOT inside the [firstLitColumn, lastLitColumn] x all-rows
  //! rectangle, plus holes inside it. 0 means the silhouette is exactly that
  //! rectangle, which is what oracle (D) predicts.
  long silhouetteDefects = -1;
};

//! CSF geometry producer -> GeometryFilterNode(shift) -> raw raster -> sink.
//! One offscreen render on `api`; collects the dispatch payload and the pixels.
FilterRun run_filter_chain(score::gfx::GraphicsApi api, float shift)
{
  FilterRun r;
  r.backend = backend_name(api);

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    const int csf = p.addCsf(corpus("syn-geo-producer.cs"));
    if(csf < 0)
    {
      r.error = p.error();
      return;
    }

    std::string ferr;
    auto owned = make_geometry_filter(
        corpus("syn-geofilter-shift.glsl"), kFilterIndex, ferr);
    if(!owned)
    {
      r.error = ferr;
      return;
    }
    auto* filterNode = owned.get();
    const int filt = p.addNode(std::move(owned));
    if(filt < 0)
    {
      r.error = p.error();
      return;
    }

    // The proven pairing for syn-geo-producer.cs (SyntheticFeatures.cpp:344-355
    // asserts G > 128 / R < 32 through it), so "lit == G > 128" is the fixture's
    // own convention rather than something invented here.
    const int raster = p.addRaster(
        corpus("syn-raster-single.vs"), corpus("syn-raster-single.fs"));
    if(raster < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink(kSize);

    auto* fin = p.nodeGeometryIn(filt, 0);
    auto* fout = p.nodeGeometryOut(filt, 0);
    if(!fin || !fout)
    {
      r.error = "the geometry filter exposes no Geometry in/out port";
      return;
    }
    p.wire(p.geometryOut(csf, 0), fin);
    p.wire(fout, p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    r.inputPorts = filterNode->input.size();
    r.outputPorts = filterNode->output.size();
    r.materialSize = filterNode->m_materialSize;
    r.filterOwnNodeId = filterNode->nodeId;

    // Oracle (B): port 0 is the Geometry inlet, port 1 the "shift" control
    // (GeometryFilterNode.cpp:186 then :24). The overload lives on ProcessNode
    // and is HIDDEN by GeometryFilterNode's own process() declarations
    // (GeometryFilterNode.hpp:39-41), hence the explicit static_cast -- the same
    // shape as score_test/Gfx.hpp's setControl (Gfx.hpp:462-470).
    if(filterNode->input.size() < 2)
    {
      r.error = "the geometry filter exposes no control inlet for `shift`";
      return;
    }
    static_cast<score::gfx::ProcessNode&>(*filterNode)
        .process(int32_t(1), ossia::value{shift});

    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      if(!r.skipped)
        r.error = p.error();
      return;
    }
    r.backend = p.backend();

    p.render(kFrames);

    // The bytes GeometryFilterNodeRenderer::update uploads every frame
    // (GeometryFilterNodeRenderer.cpp:51-52 reads exactly this pointer).
    if(filterNode->m_material_data && filterNode->m_materialSize >= int(sizeof(float)))
      std::memcpy(&r.materialShift, filterNode->m_material_data.get(), sizeof(float));

    // The Dynamic|UniformBuffer allocated at GeometryFilterNodeRenderer.cpp:34-35.
    // Node::renderedNodes is a public member (Node.hpp:114).
    for(auto& rn : filterNode->renderedNodes)
    {
      if(auto* gfr
         = dynamic_cast<score::gfx::GeometryFilterNodeRenderer*>(rn.second))
        if(auto* ubo = gfr->material())
        {
          r.uboPresent = true;
          r.uboSize = int(ubo->size());
        }
    }

    // What runInitialPasses pushed into the consumer this frame
    // (GeometryFilterNodeRenderer.cpp:113 -> NodeRenderer::process(port, spec),
    // read back through the public NodeRenderer::findGeometryByPort,
    // NodeRenderer.hpp:146-152).
    for(auto& rn : p.isf(raster)->renderedNodes)
    {
      const ossia::geometry_spec* g = rn.second->findGeometryByPort(0);
      if(!g)
        continue;
      r.consumerSawGeometry = true;
      if(!g->filters)
        continue;
      r.consumerSawFilterList = true;
      r.filterCount = g->filters->filters.size();
      if(r.filterCount > 0)
      {
        const auto& f = g->filters->filters.front();
        r.filterNodeId = f.node_id;
        r.filterId = f.filter_id;
        r.filterDirty = f.dirty_index;
        r.filterShader = f.shader;
      }
    }

    // Oracle (D): the silhouette.
    const auto img = p.readback(sink);
    r.width = img.width;
    r.height = img.height;
    if(img.valid())
    {
      for(int x = 0; x < img.width; ++x)
      {
        bool any = false;
        for(int y = 0; y < img.height; ++y)
          if(img.at(x, y)[1] > 128)
          {
            ++r.lit;
            any = true;
          }
        if(any)
        {
          if(r.firstLitColumn < 0)
            r.firstLitColumn = x;
          r.lastLitColumn = x;
        }
      }
      // Distance from the predicted rectangle: any lit pixel outside
      // [first,last] x [0,height), or any unlit pixel inside it.
      r.silhouetteDefects = 0;
      if(r.firstLitColumn >= 0)
      {
        for(int x = 0; x < img.width; ++x)
        {
          const bool inside = (x >= r.firstLitColumn && x <= r.lastLitColumn);
          for(int y = 0; y < img.height; ++y)
            if((img.at(x, y)[1] > 128) != inside)
              ++r.silhouetteDefects;
        }
      }
    }

    r.built = true;
  });

  return r;
}

//! Common preamble for the three rendering cases.
void require_ran(const FilterRun& r)
{
  INFO("backend=" << r.backend);
  INFO("error='" << r.error << "'");
  REQUIRE(r.error.empty());
  REQUIRE(r.built);
}
} // namespace

// -----------------------------------------------------------------------------
// 1. CPU only: the program the filter publishes. No RHI, so this leg never
//    skips -- it is the part of P2-9 that is verifiable everywhere.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a geometry filter compiles to a per-node process_vertex program",
    "[gfx][geometryfilter][geometry]")
{
  struct
  {
    bool built = false;
    std::string error;
    std::string shader;
    std::size_t inputs = 0;
    std::size_t outputs = 0;
    int materialSize = -1;
  } o;

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string err;
    auto node = make_geometry_filter(
        corpus("syn-geofilter-shift.glsl"), kFilterIndex, err);
    if(!node)
    {
      o.error = err;
      return;
    }
    o.shader = node->m_shader;
    o.inputs = node->input.size();
    o.outputs = node->output.size();
    o.materialSize = node->m_materialSize;
    o.built = true;
  });

  INFO("error='" << o.error << "'");
  REQUIRE(o.error.empty());
  REQUIRE(o.built);

  // Oracle (B). GeometryFilterNode.cpp:186-187 + :24 for the single float INPUT
  // declared at syn-geofilter-shift.glsl:8; ISFVisitors.hpp:153 for the size.
  CHECK(o.inputs == 2u);  // [0] Geometry, [1] "shift"
  CHECK(o.outputs == 1u); // [0] Geometry
  CHECK(o.materialSize == 4);

  // Oracle (A), piecewise first so a whitespace drift is diagnosable, then whole.
  INFO("published program:\n" << o.shader);
  const std::string n = std::to_string(kFilterIndex);

  // The UBO block libisf synthesises (isf.cpp:3228-3255).
  CHECK(
      o.shader.find("layout(std140, binding = %next%) uniform filter_" + n + "_t {")
      != std::string::npos);
  CHECK(o.shader.find("\n  float shift;\n") != std::string::npos);
  CHECK(o.shader.find("} filter_" + n + ";") != std::string::npos);

  // The binding is deliberately NOT resolved here: the consumer assigns it
  // (ModelDisplayNode.cpp:1164-1165). A filter that arrived with a concrete
  // binding would collide with the consumer's own bindings 0..3.
  CHECK(o.shader.find("%next%") != std::string::npos);
  // ...but the node index IS resolved (GeometryFilterNode.cpp:183).
  CHECK(o.shader.find("%node%") == std::string::npos);

  // The mangling that lets two filters coexist on one mesh (isf.cpp:3219-3224).
  CHECK(o.shader.find("void process_vertex_" + n + "(") != std::string::npos);
  CHECK(o.shader.find("void process_vertex(") == std::string::npos);
  CHECK(o.shader.find("this_filter") == std::string::npos);

  // The delta itself: corpus body syn-geofilter-shift.glsl:14 after rewriting.
  CHECK(o.shader.find("position.x += filter_" + n + ".shift;") != std::string::npos);

  // And the whole thing, byte for byte.
  CHECK(o.shader == expected_filter_program(kFilterIndex));
}

// -----------------------------------------------------------------------------
// 2. The dispatch: the program and the filter's identity reach the consumer's
//    renderer during a real offscreen render, i.e. on the GPU thread.
//    NEGATIVE CONTROL NC-1 reddens exactly this case.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a geometry filter publishes its vertex program to the consumer on the "
    "render thread",
    "[gfx][geometryfilter][geometry]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const FilterRun r = run_filter_chain(api, 0.5f);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  require_ran(r);

  // The upstream CSF must have reached the consumer at all, otherwise the
  // filter assertions below would be vacuously "not seen".
  REQUIRE(r.consumerSawGeometry);
  REQUIRE(r.consumerSawFilterList);

  // Oracle (C). GeometryFilterNodeRenderer.cpp:110-111 pushes ONE entry onto a
  // list its copy loop (:100-108) left empty, because syn-geo-producer.cs
  // publishes no filters of its own.
  INFO("published shader:\n" << r.filterShader);
  REQUIRE(r.filterCount == 1u);
  CHECK(r.filterNodeId == r.filterOwnNodeId);
  CHECK(r.filterId == int64_t(kFilterIndex));
  CHECK(r.filterDirty == 1);
  CHECK(r.filterShader == expected_filter_program(kFilterIndex));

  // The delta reached GPU-visible memory: a 4-byte Dynamic|UniformBuffer
  // (GeometryFilterNodeRenderer.cpp:34-35, sized by oracle B) re-uploaded every
  // frame from m_material_data (:51-52). 0.5f is exactly representable, so ==.
  CHECK(r.materialSize == 4);
  CHECK(r.materialShift == 0.5f);
  CHECK(r.uboPresent);
  CHECK(r.uboSize == 4);
}

// -----------------------------------------------------------------------------
// 3. The drawing control for case 4: with the delta at zero the chain paints the
//    full-viewport silhouette exactly. A displacement oracle is vacuous if
//    nothing drew, and this is also the leg that stays green under NC-1 (it
//    never touches the filter list) -- the asymmetry is what makes NC-1 sharp.
// -----------------------------------------------------------------------------
TEST_CASE(
    "the geometry filter chain draws the undisplaced silhouette exactly",
    "[gfx][geometryfilter][geometry]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const FilterRun r = run_filter_chain(api, 0.0f);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  require_ran(r);

  REQUIRE(r.width == kSize.width());
  REQUIRE(r.height == kSize.height());

  // Oracle (D) at s = 0: the producer's triangle contains the whole viewport,
  // so every pixel is lit.
  INFO(
      "first=" << r.firstLitColumn << " last=" << r.lastLitColumn
               << " lit=" << r.lit << " defects=" << r.silhouetteDefects);
  CHECK(r.firstLitColumn == 0);
  CHECK(r.lastLitColumn == kSize.width() - 1);
  CHECK(r.lit == long(kSize.width()) * long(kSize.height())); // 64*64 == 4096
  CHECK(r.silhouetteDefects == 0);
}

// -----------------------------------------------------------------------------
// 4. THE CASE ITSELF -- P2-9's oracle. EXPECTED TO FAIL today.
//
// Not because of anything in GeometryFilterNode: the filter publishes the right
// program to the right consumer with the right delta (case 2 proves all three).
// It fails because score_plugin_gfx has no consumer that APPLIES a geometry
// filter. RenderedRawRasterPipelineNode never reads mesh.filters -- the only
// mention in the whole file is the FIXME at
// RenderedRawRasterPipelineNode.cpp:2630 -- so the only splice site in the tree
// is ModelDisplayNode.cpp:1157-1176, in score-plugin-threedim, which
// score_add_gfx_test does not link. The silhouette therefore stays at s = 0's
// full viewport: first lit column 0 instead of 16, 4096 lit pixels instead of
// 3072.
//
// This also explains tests/gfx/CroustiCpuNodes.cpp's "a geometry filter
// displaces the mesh it is given", which is [!shouldfail] with a comment
// calling the failure "still unexplained": that chain also ends in
// p.addRaster. Same cause, and the note there should be updated.
//
// TO MAKE THIS GREEN, either:
//   (a) give RenderedRawRasterPipelineNode the ModelDisplayNode.cpp:1157-1176
//       splice plus the ModelDisplayNode.cpp:858-885 material binding, or
//   (b) re-register this file against score_plugin_threedim (the
//       score_plugin_hidden_sources shape at tests/gfx/CMakeLists.txt:388-393)
//       and swap the raw raster for a ModelDisplayNode.
// Once green, NC-2 ("zero the delta", GeometryFilterNodeRenderer.cpp:52) becomes
// the runnable negative control for this case.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a geometry filter shifts the drawn silhouette by exactly the delta",
    "[gfx][geometryfilter][geometry][!shouldfail]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const FilterRun r = run_filter_chain(api, 0.5f);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  require_ran(r);

  REQUIRE(r.width == kSize.width());
  REQUIRE(r.height == kSize.height());

  // Oracle (D) at s = 0.5, W = 64:
  //   lit  <=>  px >= s*W/2 - 0.5 = 15.5  <=>  px >= 16
  //   so columns 16..63, all 64 rows: 48 * 64 = 3072 pixels.
  constexpr int kFirst = 16;                              // 0.5 * 64 / 2
  constexpr int kLast = 63;                               // W - 1
  constexpr long kLitPixels = long(kLast - kFirst + 1) * 64; // 3072
  INFO(
      "first=" << r.firstLitColumn << " last=" << r.lastLitColumn
               << " lit=" << r.lit << " defects=" << r.silhouetteDefects);
  CHECK(r.firstLitColumn == kFirst);
  CHECK(r.lastLitColumn == kLast);
  CHECK(r.lit == kLitPixels);
  CHECK(r.silhouetteDefects == 0);
}
