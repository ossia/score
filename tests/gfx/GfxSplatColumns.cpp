// =============================================================================
// Gaussian splats of any column layout, read by name, without a copy.
//
// The Asset Loader keeps a splat file's rows as they are and lists their
// columns (name, scalar type, byte offset); the Scene Preprocessor publishes
// every column as a named attribute that is a view into its raw_splats buffer;
// a compute shader declaring COLUMNS reads them where they are through the
// generated ISF_COLUMN accessors. One decode shader
// (corpus/splat-columns/01_DecodeColumns.cs) then renders every layout.
//
// (a) LAYOUTS. The same logical splats, written as: the INRIA export (62
//     floats, "3dgs.classic"), no normals (59, Postshot / cluster.fly), SH
//     degree 0, 1 and 2, every column double, a shuffled order with junk
//     columns of six scalar types, an antimatter15 .splat and an .spz. Each
//     goes Asset Loader -> Scene Preprocessor -> Flattened Scene Filter (on
//     its own format id) -> 01_DecodeColumns.cs -> 02_DrawSplat and is
//     compared with the INRIA file, SH truncated to the layout's degree,
//     through the same chain. The INRIA file is also drawn through the struct
//     decoder (01_Decode.cs, Splat3DGS), the 3dgs.score path, with and
//     without the column list on the cloud: those two must be identical, and
//     equal to the by-name decode.
//     Negative control: SH degree 0 against the full-degree INRIA reference
//     must differ, so the comparison can see the SH terms.
//
// (b) NO COPY. Reading the INRIA cloud by name allocates no buffer for the
//     rows: the COLUMNS data block of the CSF is the upstream raw_splats
//     buffer itself (same QRhiBuffer, not owned), the CSF owns exactly one more
//     buffer than with the struct decoder (the column table, 16 bytes per
//     declared column), the preprocessor uploads the same rows once, and on
//     Vulkan the device memory in use differs by less than the table's
//     allocation granule, where one copied column would add N * 4 bytes.
//
// The perf comparison on large files is the hidden case "[.splat-perf]"
// (SPLAT_PERF_PLY=<file.ply>).
//
// Run: SCORE_TEST_API=opengl|vulkan ctest -R gfx_splat_columns
// =============================================================================

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#define private public
#include <Gfx/Graph/RenderedCSFNode.hpp>
#undef private

#include <Threedim/AssetLoader.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <load-spz.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 96;
constexpr int kGrid = 14; // kGrid^2 splats

QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/splat-columns/") + QString::fromUtf8(f);
}

// ---------------------------------------------------------------------------
// The logical splats, in INRIA terms: f_rest channel-major, 15 per channel.
// ---------------------------------------------------------------------------
struct Splat
{
  float pos[3];
  float nrm[3];
  float dc[3];
  float rest[45];
  float opacity;
  float scale[3];
  float rot[4]; // w x y z
};

struct Lcg
{
  uint32_t s;
  float next() noexcept
  {
    s = s * 1664525u + 1013904223u;
    return float(s >> 8) / float(1u << 24);
  }
  float range(float a, float b) noexcept { return a + (b - a) * next(); }
};

std::vector<Splat> makeSplats(int grid)
{
  std::vector<Splat> v;
  Lcg r{12345u};
  for(int j = 0; j < grid; ++j)
    for(int i = 0; i < grid; ++i)
    {
      Splat s{};
      s.pos[0] = -0.9f + 1.8f * (i + 0.5f) / grid + r.range(-0.03f, 0.03f);
      s.pos[1] = -0.7f + 1.4f * (j + 0.5f) / grid + r.range(-0.03f, 0.03f);
      s.pos[2] = r.range(-0.4f, 0.4f);
      for(float& n : s.nrm)
        n = r.range(-1.f, 1.f);
      for(float& c : s.dc)
        c = r.range(-1.6f, 1.6f);
      for(float& c : s.rest)
        c = r.range(-0.45f, 0.45f);
      s.opacity = r.range(0.f, 3.f);
      for(float& c : s.scale)
        c = r.range(-3.4f, -2.5f);
      float q[4], n = 0.f;
      for(float& c : q)
      {
        c = r.range(-1.f, 1.f);
        n += c * c;
      }
      n = std::sqrt(n);
      for(int k = 0; k < 4; ++k)
        s.rot[k] = q[k] / n;
      v.push_back(s);
    }
  return v;
}

// SH coefficients per channel for a degree.
constexpr int restPerChannel(int deg) noexcept
{
  return deg >= 3 ? 15 : deg == 2 ? 8 : deg == 1 ? 3 : 0;
}

std::vector<Splat> truncateSH(std::vector<Splat> v, int deg)
{
  const int K = restPerChannel(deg);
  for(auto& s : v)
    for(int c = 0; c < 3; ++c)
      for(int j = K; j < 15; ++j)
        s.rest[c * 15 + j] = 0.f;
  return v;
}

// ---------------------------------------------------------------------------
// PLY writer: any column list, any scalar type, binary little endian.
// ---------------------------------------------------------------------------
struct Column
{
  std::string name;
  std::string type; // PLY scalar type name
};

// A column's value for a splat. `K` is the layout's SH coefficients per
// channel: f_rest_<c*K + j> is coefficient j of channel c.
double columnValue(const Splat& s, const std::string& n, int K, int row)
{
  auto idx = [&](const char* prefix) { return std::stoi(n.substr(std::strlen(prefix))); };
  if(n == "x") return s.pos[0];
  if(n == "y") return s.pos[1];
  if(n == "z") return s.pos[2];
  if(n == "nx") return s.nrm[0];
  if(n == "ny") return s.nrm[1];
  if(n == "nz") return s.nrm[2];
  if(n.rfind("f_dc_", 0) == 0) return s.dc[idx("f_dc_")];
  if(n.rfind("f_rest_", 0) == 0)
  {
    const int k = idx("f_rest_");
    return s.rest[(k / K) * 15 + (k % K)];
  }
  if(n == "opacity") return s.opacity;
  if(n.rfind("scale_", 0) == 0) return s.scale[idx("scale_")];
  if(n.rfind("rot_", 0) == 0) return s.rot[idx("rot_")];
  // Junk: row-dependent values a decoder reading the wrong column would show.
  return double((row * 37 + int(n.size()) * 11) % 200) - 60.0;
}

void appendScalar(std::string& out, const std::string& type, double v)
{
  auto put = [&](auto x) {
    char b[sizeof(x)];
    std::memcpy(b, &x, sizeof(x));
    out.append(b, sizeof(x));
  };
  if(type == "float") put(float(v));
  else if(type == "double") put(v);
  else if(type == "uchar") put(uint8_t(std::clamp(v, 0.0, 255.0)));
  else if(type == "char") put(int8_t(std::clamp(v, -128.0, 127.0)));
  else if(type == "short") put(int16_t(v));
  else if(type == "ushort") put(uint16_t(std::max(v, 0.0)));
  else if(type == "int") put(int32_t(v));
  else if(type == "uint") put(uint32_t(std::max(v, 0.0)));
}

std::string writeFile(const QString& dir, const QString& name, const std::string& bytes)
{
  const QString path = dir + "/" + name;
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(bytes.data(), qint64(bytes.size()));
  return path.toStdString();
}

std::string makePly(const std::vector<Splat>& splats, const std::vector<Column>& cols, int K)
{
  std::string f = "ply\nformat binary_little_endian 1.0\nelement vertex "
                  + std::to_string(splats.size()) + "\n";
  for(auto& c : cols)
    f += "property " + c.type + " " + c.name + "\n";
  f += "end_header\n";
  for(std::size_t r = 0; r < splats.size(); ++r)
    for(auto& c : cols)
      appendScalar(f, c.type, columnValue(splats[r], c.name, K, int(r)));
  return f;
}

std::vector<Column> inriaColumns(bool normals, int deg, const char* type = "float")
{
  std::vector<Column> c{{"x", type}, {"y", type}, {"z", type}};
  if(normals)
    for(const char* n : {"nx", "ny", "nz"})
      c.push_back({n, type});
  for(int i = 0; i < 3; ++i)
    c.push_back({"f_dc_" + std::to_string(i), type});
  for(int i = 0; i < 3 * restPerChannel(deg); ++i)
    c.push_back({"f_rest_" + std::to_string(i), type});
  c.push_back({"opacity", type});
  for(int i = 0; i < 3; ++i)
    c.push_back({"scale_" + std::to_string(i), type});
  for(int i = 0; i < 4; ++i)
    c.push_back({"rot_" + std::to_string(i), type});
  return c;
}

// antimatter15 convert.py.
std::string makeSplatBinary(const std::vector<Splat>& splats)
{
  constexpr double SH_C0 = 0.28209479177387814;
  std::string f;
  for(const auto& s : splats)
  {
    for(float p : s.pos)
      appendScalar(f, "float", p);
    for(float sc : s.scale)
      appendScalar(f, "float", std::exp(sc));
    for(float c : s.dc)
      appendScalar(f, "uchar", std::round((0.5 + SH_C0 * c) * 255.0));
    appendScalar(f, "uchar", std::round(255.0 / (1.0 + std::exp(-s.opacity))));
    for(float q : s.rot)
      appendScalar(f, "uchar", std::round(q * 128.0 + 128.0));
  }
  return f;
}

std::string makeSpz(const std::vector<Splat>& splats, int deg)
{
  spz::GaussianCloud g;
  g.numPoints = int(splats.size());
  g.shDegree = deg;
  const int K = restPerChannel(deg);
  for(const auto& s : splats)
  {
    g.positions.insert(g.positions.end(), s.pos, s.pos + 3);
    g.scales.insert(g.scales.end(), s.scale, s.scale + 3);
    g.rotations.insert(g.rotations.end(), {s.rot[1], s.rot[2], s.rot[3], s.rot[0]});
    g.alphas.push_back(s.opacity);
    g.colors.insert(g.colors.end(), s.dc, s.dc + 3);
    for(int j = 0; j < K; ++j)
      for(int c = 0; c < 3; ++c)
        g.sh.push_back(s.rest[c * 15 + j]);
  }
  spz::PackOptions o;
  o.from = spz::CoordinateSystem::RDF;
  std::vector<uint8_t> out;
  if(!spz::saveSpz(g, o, &out))
    return {};
  return std::string(out.begin(), out.end());
}

// ---------------------------------------------------------------------------
// Scene producer: publishes the loader's scene on a Scene port, as
// GfxSplatRender.cpp does.
// ---------------------------------------------------------------------------
struct AssetSceneNode final : score::gfx::ProcessNode
{
  ossia::scene_spec spec;
  char key_tag{};
  explicit AssetSceneNode(ossia::scene_spec s)
      : spec{std::move(s)}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct AssetSceneRenderer final : score::gfx::NodeRenderer
{
  AssetSceneNode& self;
  explicit AssetSceneRenderer(const AssetSceneNode& n)
      : NodeRenderer{n}
      , self{const_cast<AssetSceneNode&>(n)}
  {
  }
  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
  }
  void release(score::gfx::RenderList&) override { m_initialized = false; }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override { }
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    auto* sink = edge.sink;
    if(!self.spec.state || !sink || !sink->node)
      return;
    auto rn = sink->node->renderedNodes.find(&renderer);
    if(rn == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    rn->second->process(int(it - sink->node->input.begin()), self.spec, &self.key_tag);
  }
};

score::gfx::NodeRenderer* AssetSceneNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new AssetSceneRenderer{*this};
}

// ---------------------------------------------------------------------------
// Loading, through the shipped AssetLoader dispatch.
// ---------------------------------------------------------------------------
struct Loaded
{
  ossia::scene_spec scene;
  ossia::primitive_cloud_component_ptr cloud;
  double ms{};
};

ossia::primitive_cloud_component_ptr findCloud(const ossia::scene_node& n)
{
  if(!n.children)
    return {};
  for(const auto& p : *n.children)
  {
    if(auto* c = ossia::get_if<ossia::primitive_cloud_component_ptr>(&p); c && *c)
      return *c;
    if(auto* sn = ossia::get_if<ossia::scene_node_ptr>(&p); sn && *sn)
      if(auto c = findCloud(**sn))
        return c;
  }
  return {};
}

// The scene with its cloud replaced (same rows, other metadata).
ossia::scene_node_ptr replaceCloud(
    const ossia::scene_node_ptr& n, const ossia::primitive_cloud_component_ptr& cloud)
{
  if(!n || !n->children)
    return n;
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  for(const auto& p : *n->children)
  {
    if(ossia::get_if<ossia::primitive_cloud_component_ptr>(&p))
      children->emplace_back(cloud);
    else if(auto* sn = ossia::get_if<ossia::scene_node_ptr>(&p))
      children->emplace_back(replaceCloud(*sn, cloud));
    else
      children->push_back(p);
  }
  auto out = std::make_shared<ossia::scene_node>(*n);
  out->children = std::move(children);
  return out;
}

ossia::scene_spec withCloud(
    const ossia::scene_spec& in, const ossia::primitive_cloud_component_ptr& cloud)
{
  auto st = std::make_shared<ossia::scene_state>(*in.state);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  for(const auto& r : *in.state->roots)
    roots->push_back(replaceCloud(r, cloud));
  st->roots = std::move(roots);
  st->version++;
  st->dirty_index++;
  ossia::scene_spec out = in;
  out.state = std::move(st);
  return out;
}

// The cloud as HEAD published it: no column list.
ossia::scene_spec withoutColumns(const Loaded& l)
{
  auto c = std::make_shared<ossia::primitive_cloud_component>(*l.cloud);
  c->columns.clear();
  c->columns_hash = 0;
  return withCloud(l.scene, c);
}

Loaded load(const std::string& path)
{
  Loaded out;
  QFile f{QString::fromStdString(path)};
  if(!f.open(QIODevice::ReadOnly))
    return out;
  const QByteArray raw = f.readAll();
  halp::text_file_view tv;
  tv.filename = path;
  tv.bytes = std::string_view{raw.constData(), std::size_t(raw.size())};
  QElapsedTimer t;
  t.start();
  auto apply = Threedim::AssetLoader::ins::asset_t::process(tv);
  if(!apply)
    return out;
  Threedim::AssetLoader loader;
  apply(loader);
  loader();
  out.ms = t.nsecsElapsed() / 1e6;
  out.scene = loader.outputs.scene_out.scene;
  if(out.scene.state && out.scene.state->roots)
    for(const auto& r : *out.scene.state->roots)
      if(r)
        if((out.cloud = findCloud(*r)))
          break;
  return out;
}

// ---------------------------------------------------------------------------
// One chain run.
// ---------------------------------------------------------------------------
struct Run
{
  bool skipped{};
  std::string skip;
  std::string error;
  std::string backend;
  ReadbackImage img;

  // No-copy evidence.
  QRhiBuffer* rawSplats{};
  QRhiBuffer* columnData{};
  bool columnDataOwned{true};
  QRhiBuffer* splatAttr{};
  int csfOwnedBuffers{};
  int64_t csfOwnedBytes{};
  uint64_t rowUploads{}, rowBytes{}, tables{}, tableBytes{};
  bool haveStats{};
  quint32 allocCount{};
  quint64 usedBytes{};
  double frameMs{};
  double frameMinMs{};
  double warmMs{};
  //! The CSF compiled its column reads for the upstream layout.
  bool specialized{};
};

struct ChainSpec
{
  ossia::scene_spec scene;
  std::string formatId;
  QString decoder;
  int warmFrames{6};
  int timedFrames{0};
  QSize size{kSize, kSize};
  //! false: the CSF reads the column table instead of a layout compiled in.
  bool specialize{true};
};

Run runChain(score::gfx::GraphicsApi api, const ChainSpec& spec)
{
  Run out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addNode(std::make_unique<AssetSceneNode>(spec.scene));
    const int preproc = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    auto filterNode = std::make_unique<score::gfx::FlattenedSceneFilterNode>();
    filterNode->m_mode = 12;
    filterNode->m_match = 0;
    filterNode->m_match_str = spec.formatId;
    const int filter = p.addNode(std::move(filterNode));
    const int csf = p.addCsf(spec.decoder);
    const int raster = p.addRaster(corpus("02_DrawSplat.vs"), corpus("02_DrawSplat.frag"));
    if(producer < 0 || preproc < 0 || filter < 0 || csf < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(producer, 0), p.nodeSceneIn(preproc, 0));
    p.wire(p.nodeGeometryOut(preproc, 0), p.nodeGeometryIn(filter, 0));
    p.wire(p.nodeGeometryOut(filter, 0), p.geometryIn(csf, 0));
    p.wire(p.geometryOut(csf, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink(spec.size);
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();
    for(auto& [rl, nr] : p.isf(csf)->renderedNodes)
      if(auto* r = dynamic_cast<score::gfx::RenderedCSFNode*>(nr))
        r->m_specializeColumns = spec.specialize;
    {
      QElapsedTimer t;
      t.start();
      p.render(spec.warmFrames);
      out.warmMs = t.nsecsElapsed() / 1e6;
    }
    if(spec.timedFrames > 0)
    {
      // Each frame waits for its readback, so a frame's wall time bounds its GPU
      // time; the median and the minimum are robust to the load of the machine.
      std::vector<double> ms;
      for(int f = 0; f < spec.timedFrames; ++f)
      {
        QElapsedTimer t;
        t.start();
        p.render(1);
        ms.push_back(t.nsecsElapsed() / 1e6);
      }
      std::sort(ms.begin(), ms.end());
      out.frameMs = ms[ms.size() / 2];
      out.frameMinMs = ms.front();
    }
    out.img = p.readback(sink);

    auto* csfNode = p.isf(csf);
    for(auto& [rl, nr] : csfNode->renderedNodes)
    {
      auto* r = dynamic_cast<score::gfx::RenderedCSFNode*>(nr);
      if(!r)
        continue;
      out.specialized = r->m_specializeColumns && !r->m_columnSpec.empty()
                        && !r->m_columnSpec[0].empty();
      if(r->geometry.meshes && !r->geometry.meshes->meshes.empty())
      {
        const auto& m = r->geometry.meshes->meshes[0];
        if(auto* aux = m.find_auxiliary("raw_splats"))
          if(auto* g = ossia::get_if<ossia::geometry::gpu_buffer>(&m.buffers[aux->buffer].data))
            out.rawSplats = static_cast<QRhiBuffer*>(g->handle);
      }
      for(auto& b : r->m_geometryBindings)
      {
        for(auto& a : b.attribute_ssbos)
        {
          if(a.name == "splat")
            out.splatAttr = a.buffer;
          if(a.owned && a.buffer)
          {
            out.csfOwnedBuffers++;
            out.csfOwnedBytes += a.buffer->size();
          }
          if(a.read_buffer && a.read_buffer != a.buffer)
          {
            out.csfOwnedBuffers++;
            out.csfOwnedBytes += a.read_buffer->size();
          }
        }
        for(auto& a : b.auxiliary_ssbos)
        {
          if(a.column_block == 1)
          {
            out.columnData = a.buffer;
            out.columnDataOwned = a.owned;
          }
          if(a.owned && a.buffer)
          {
            out.csfOwnedBuffers++;
            out.csfOwnedBytes += a.buffer->size();
          }
        }
      }
      const auto& reg = rl->registry();
      out.rowUploads = reg.primitiveCloudUploads.rows;
      out.rowBytes = reg.primitiveCloudUploads.rowBytes;
      out.tables = reg.columnTableUploads.tables;
      out.tableBytes = reg.columnTableUploads.bytes;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
      if(api == score::gfx::Vulkan && rl->state.rhi)
      {
        const QRhiStats st = rl->state.rhi->statistics();
        out.haveStats = true;
        out.allocCount = st.allocCount;
        out.usedBytes = st.usedBytes;
      }
#endif
    }
  });
  return out;
}

// ---------------------------------------------------------------------------
// Image comparison.
// ---------------------------------------------------------------------------
struct Diff
{
  int maxAbs{};
  double meanAbs{};
  int litA{}, litB{};
};

Diff compare(const ReadbackImage& a, const ReadbackImage& b)
{
  Diff d;
  if(!a.valid() || !b.valid() || a.width != b.width || a.height != b.height)
  {
    d.maxAbs = 256;
    return d;
  }
  double sum = 0;
  for(int y = 0; y < a.height; ++y)
    for(int x = 0; x < a.width; ++x)
    {
      const auto pa = a.at(x, y);
      const auto pb = b.at(x, y);
      bool la = false, lb = false;
      for(int c = 0; c < 4; ++c)
      {
        const int e = std::abs(int(pa[c]) - int(pb[c]));
        d.maxAbs = std::max(d.maxAbs, e);
        sum += e;
        if(c < 3)
        {
          la |= pa[c] != 0;
          lb |= pb[c] != 0;
        }
      }
      d.litA += la;
      d.litB += lb;
    }
  d.meanAbs = sum / (4.0 * a.width * a.height);
  return d;
}

// Needs the application compute_shader_skip_reason probes the GL context in.
const char* computeSkip(score::gfx::GraphicsApi api)
{
  const char* why = nullptr;
  score::test::run_in_gui_app(
      [&](const score::GUIApplicationContext&) { why = compute_shader_skip_reason(api); });
  return why;
}

struct Layout
{
  const char* name;
  std::string file;      // written on disk
  std::string formatId;  // expected autodetected tag, routed on
  int degree;            // SH information the layout carries
  int maxAbs;            // tolerance against the INRIA reference
  double meanAbs;
};

} // namespace

TEST_CASE(
    "every gaussian layout renders by column name like the INRIA file",
    "[gfx][threedim][splat][primitivecloud][columns]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  if(const char* why = computeSkip(api))
    SKIP(why);

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const QString dir = tmp.path();
  const auto splats = makeSplats(kGrid);

  // The reordered layout: INRIA columns shuffled, junk of every scalar type
  // between them. PLY rows align each scalar to its size.
  std::vector<Column> shuffled = inriaColumns(true, 3);
  {
    Lcg r{777u};
    for(std::size_t i = shuffled.size() - 1; i > 0; --i)
      std::swap(shuffled[i], shuffled[std::size_t(r.next() * (i + 1)) % (i + 1)]);
    // "cloud_id" is the preprocessor's attribute: the column is published
    // as cloud_id_2 and must not shadow it.
    const std::vector<Column> junk{{"junk_u8", "uchar"}, {"junk_d", "double"},
                                   {"splat", "short"},   {"junk_c", "char"},
                                   {"cloud_id", "int"},  {"junk_us", "ushort"}};
    for(std::size_t k = 0; k < junk.size(); ++k)
      shuffled.insert(shuffled.begin() + 1 + 9 * k, junk[k]);
  }

  std::vector<Layout> layouts{
      {"inria-62", writeFile(dir, "inria.ply", makePly(splats, inriaColumns(true, 3), 15)),
       "3dgs.classic", 3, 0, 0.0},
      {"no-normals-59",
       writeFile(dir, "nonormals.ply", makePly(splats, inriaColumns(false, 3), 15)), "3dgs", 3,
       0, 0.0},
      {"sh0", writeFile(dir, "sh0.ply", makePly(splats, inriaColumns(false, 0), 0)), "3dgs", 0, 0,
       0.0},
      {"sh1", writeFile(dir, "sh1.ply", makePly(splats, inriaColumns(false, 1), 3)), "3dgs", 1, 0,
       0.0},
      {"sh2", writeFile(dir, "sh2.ply", makePly(splats, inriaColumns(true, 2), 8)), "3dgs", 2, 0,
       0.0},
      // A double rounds to the same float, so it decodes to the same bits.
      {"double", writeFile(dir, "double.ply", makePly(splats, inriaColumns(true, 3, "double"), 15)),
       "3dgs", 3, 0, 0.0},
      {"reordered-junk", writeFile(dir, "reordered.ply", makePly(splats, shuffled, 15)), "3dgs", 3,
       0, 0.0},
      // 8-bit colour, opacity and rotation: quantisation error.
      {".splat", writeFile(dir, "cloud.splat", makeSplatBinary(splats)), "3dgs.splat-binary", 0,
       40, 1.5},
      // Fixed-point positions, 8-bit SH: quantisation error.
      {".spz", writeFile(dir, "cloud.spz", makeSpz(splats, 3)), "3dgs.classic", 3, 60, 2.5},
  };

  const QString byName = corpus("01_DecodeColumns.cs");
  const QString byStruct = corpus("01_Decode.cs");

  // References: the INRIA file of the same splats, SH cut to each degree.
  std::map<int, Run> reference;
  for(int deg : {0, 1, 2, 3})
  {
    const auto path = writeFile(
        dir, QString("ref%1.ply").arg(deg),
        makePly(truncateSH(splats, deg), inriaColumns(true, 3), 15));
    const auto l = load(path);
    REQUIRE(l.cloud);
    REQUIRE(l.cloud->format_id == "3dgs.classic");
    reference[deg] = runChain(api, {l.scene, "3dgs.classic", byName});
    if(reference[deg].skipped)
      SKIP(reference[deg].backend + ": " + reference[deg].skip);
    INFO("reference degree " << deg << " " << reference[deg].error);
    REQUIRE(reference[deg].error.empty());
    REQUIRE(reference[deg].img.valid());
  }
  const auto& ref3 = reference[3];
  {
    // The splats cover part of the frame, in many colours.
    const auto d = compare(ref3.img, ref3.img);
    INFO("lit pixels " << d.litA << " of " << kSize * kSize);
    REQUIRE(d.litA > kSize * kSize / 10);
    REQUIRE(d.litA < kSize * kSize * 99 / 100);
    std::set<uint32_t> colours;
    for(int y = 0; y < ref3.img.height; ++y)
      for(int x = 0; x < ref3.img.width; ++x)
      {
        const auto p = ref3.img.at(x, y);
        colours.insert(p[0] | (p[1] << 8) | (p[2] << 16));
      }
    REQUIRE(colours.size() > 500);
  }

  // The struct path (3dgs.score: 01_Decode.cs reading Splat3DGS), with and
  // without the column list on the cloud: identical, and equal to by name.
  {
    const auto l = load(layouts[0].file);
    const auto withCols = runChain(api, {l.scene, "3dgs.classic", byStruct});
    const auto withoutCols = runChain(api, {withoutColumns(l), "3dgs.classic", byStruct});
    REQUIRE(withCols.error.empty());
    REQUIRE(withoutCols.error.empty());
    const auto same = compare(withCols.img, withoutCols.img);
    std::printf(
        "[splat-columns] %s struct decode, column list vs none: max %d mean %.4f\n",
        withCols.backend.c_str(), same.maxAbs, same.meanAbs);
    CHECK(same.maxAbs == 0);
    const auto vsName = compare(withCols.img, ref3.img);
    std::printf(
        "[splat-columns] %s struct vs by-name decode: max %d mean %.4f\n",
        withCols.backend.c_str(), vsName.maxAbs, vsName.meanAbs);
    CHECK(vsName.maxAbs <= 1);
  }

  for(const auto& L : layouts)
  {
    INFO("backend " << ref3.backend << " layout " << L.name);
    REQUIRE_FALSE(L.file.empty());
    const auto l = load(L.file);
    REQUIRE(l.cloud);
    CHECK(l.cloud->format_id == L.formatId);
    CHECK(l.cloud->primitive_count == splats.size());
    CHECK_FALSE(l.cloud->columns.empty());
    // Compiled for the layout, and reading the column table.
    for(bool specialize : {true, false})
    {
      ChainSpec cs{l.scene, L.formatId, byName};
      cs.specialize = specialize;
      const auto r = runChain(api, cs);
      INFO(r.error << " specialized " << specialize);
      REQUIRE(r.error.empty());
      CHECK(r.specialized == specialize);
      const auto d = compare(r.img, reference[L.degree].img);
      std::printf(
          "[splat-columns] %s %-15s %s vs INRIA(SH%d): max %3d mean %.4f lit %d/%d\n",
          ref3.backend.c_str(), L.name, specialize ? "compiled" : "table   ", L.degree,
          d.maxAbs, d.meanAbs, d.litA, d.litB);
      CHECK(d.maxAbs <= L.maxAbs);
      CHECK(d.meanAbs <= L.meanAbs);
      CHECK(std::abs(d.litA - d.litB) <= d.litB / 50);
    }
  }

  // Negative control: the comparison sees the SH terms.
  {
    const auto d = compare(reference[0].img, ref3.img);
    std::printf(
        "[splat-columns] %s negative control SH0 vs SH3: max %d mean %.4f\n",
        ref3.backend.c_str(), d.maxAbs, d.meanAbs);
    CHECK(d.meanAbs > 1.0);
  }
}

TEST_CASE(
    "reading splat columns by name binds the preprocessor's rows, no copy",
    "[gfx][threedim][splat][primitivecloud][columns][nocopy]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  if(const char* why = computeSkip(api))
    SKIP(why);

  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());
  const auto splats = makeSplats(kGrid);
  const auto path
      = writeFile(tmp.path(), "inria.ply", makePly(splats, inriaColumns(true, 3), 15));
  const auto l = load(path);
  REQUIRE(l.cloud);

  const auto structRun = runChain(api, {withoutColumns(l), "3dgs.classic", corpus("01_Decode.cs")});
  if(structRun.skipped)
    SKIP(structRun.backend + ": " + structRun.skip);
  REQUIRE(structRun.error.empty());
  const auto colRun = runChain(api, {l.scene, "3dgs.classic", corpus("01_DecodeColumns.cs")});
  REQUIRE(colRun.error.empty());

  const int64_t rows = int64_t(splats.size()) * 62 * 4;
  constexpr int declared = 70; // COLUMNS of 01_DecodeColumns.cs
  std::printf(
      "[splat-columns] %s no-copy: struct owned %d (%lld B) rows up %llu (%llu B) vk "
      "allocs %u used %llu | columns owned %d (%lld B) rows up %llu (%llu B) tables %llu "
      "(%llu B) vk allocs %u used %llu | data==raw_splats %d owned %d\n",
      colRun.backend.c_str(), structRun.csfOwnedBuffers, (long long)structRun.csfOwnedBytes,
      (unsigned long long)structRun.rowUploads, (unsigned long long)structRun.rowBytes,
      structRun.allocCount, (unsigned long long)structRun.usedBytes, colRun.csfOwnedBuffers,
      (long long)colRun.csfOwnedBytes, (unsigned long long)colRun.rowUploads,
      (unsigned long long)colRun.rowBytes, (unsigned long long)colRun.tables,
      (unsigned long long)colRun.tableBytes, colRun.allocCount,
      (unsigned long long)colRun.usedBytes, int(colRun.columnData == colRun.rawSplats),
      int(colRun.columnDataOwned));
  INFO(
      "backend " << colRun.backend << " struct: owned " << structRun.csfOwnedBuffers << " ("
                 << structRun.csfOwnedBytes << " B), rows up " << structRun.rowUploads << " ("
                 << structRun.rowBytes << " B), vk allocs " << structRun.allocCount << " used "
                 << structRun.usedBytes << " | columns: owned " << colRun.csfOwnedBuffers
                 << " (" << colRun.csfOwnedBytes << " B), rows up " << colRun.rowUploads << " ("
                 << colRun.rowBytes << " B), tables " << colRun.tables << " ("
                 << colRun.tableBytes << " B), vk allocs " << colRun.allocCount << " used "
                 << colRun.usedBytes);

  // The struct attribute already binds raw_splats in place.
  REQUIRE(structRun.rawSplats);
  CHECK(structRun.splatAttr == structRun.rawSplats);

  // The column data block IS raw_splats: adopted, not owned, not a copy.
  REQUIRE(colRun.rawSplats);
  CHECK(colRun.columnData == colRun.rawSplats);
  CHECK_FALSE(colRun.columnDataOwned);

  // Same rows uploaded, once.
  CHECK(colRun.rowUploads == structRun.rowUploads);
  CHECK(colRun.rowBytes == structRun.rowBytes);
  CHECK(colRun.rowBytes == uint64_t(rows));

  // One table, 16 bytes per declared column, uploaded once.
  CHECK(colRun.tables == 1);
  CHECK(colRun.tableBytes == uint64_t(16 * declared));

  // The CSF owns exactly the table more. The struct decoder owns no
  // attribute buffer for its input either: both bind upstream buffers.
  CHECK(colRun.csfOwnedBuffers == structRun.csfOwnedBuffers + 1);
  CHECK(colRun.csfOwnedBytes - structRun.csfOwnedBytes == 16 * declared);

  // Device memory: within the allocation granule of the table, while a copy
  // of one column would add N * 4 bytes and a copy of the rows N * 248.
  if(colRun.haveStats && structRun.haveStats)
  {
    const auto diff = int64_t(colRun.usedBytes) - int64_t(structRun.usedBytes);
    INFO("vulkan used bytes diff " << diff);
    CHECK(std::abs(diff) <= 64 * 1024);
    CHECK(std::abs(diff) < rows);
  }
}

// ---------------------------------------------------------------------------
// Performance comparison on a large file, hidden. SPLAT_PERF_PLY: the file;
// SPLAT_PERF_FRAMES (default 60): timed frames; SPLAT_PERF_SIZE (default
// 1280): frame edge.
// ---------------------------------------------------------------------------
TEST_CASE("splat columns: load, memory, uploads and frame time", "[.splat-perf]")
{
  const QByteArray file = qgetenv("SPLAT_PERF_PLY");
  if(file.isEmpty())
    SKIP("SPLAT_PERF_PLY is not set");
  const auto api = GENERATE(from_range(platform_backends()));
  if(const char* why = computeSkip(api))
    SKIP(why);
  const int frames = qEnvironmentVariableIsSet("SPLAT_PERF_FRAMES")
                         ? qEnvironmentVariableIntValue("SPLAT_PERF_FRAMES")
                         : 60;
  const int edge = qEnvironmentVariableIsSet("SPLAT_PERF_SIZE")
                       ? qEnvironmentVariableIntValue("SPLAT_PERF_SIZE")
                       : 1280;

  const auto l = load(file.toStdString());
  REQUIRE(l.cloud);
  const std::string tag = l.cloud->format_id;

  struct Config
  {
    const char* name;
    bool columns;
    const char* decoder;
  };
  std::vector<Config> configs{
      {"HEAD: struct decode, no column list", false, "01_Decode.cs"},
      {"struct decode, column list published", true, "01_Decode.cs"},
      {"by name, 14 columns (SH0)", true, "01_DecodeColumns14.cs"},
      {"by name, 62 columns", true, "01_DecodeColumns62.cs"},
      {"by name, 70 columns (any layout)", true, "01_DecodeColumns.cs"},
  };
  if(tag != "3dgs.classic")
    configs.erase(configs.begin(), configs.begin() + 2);
  // SPLAT_PERF_ONLY=<n,...>: run these configurations only.
  if(const QByteArray only = qgetenv("SPLAT_PERF_ONLY"); !only.isEmpty())
  {
    std::vector<Config> kept;
    for(const auto& k : only.split(','))
      if(k.toInt() >= 0 && k.toInt() < (int)configs.size())
        kept.push_back(configs[k.toInt()]);
    configs = kept;
  }
  // SPLAT_PERF_EXTRA=<absolute .cs path>: one more by-name decoder.
  const QByteArray extra = qgetenv("SPLAT_PERF_EXTRA");
  if(!extra.isEmpty())
    configs.push_back({extra.constData(), true, extra.constData()});

  std::printf(
      "[splat-perf] %s: %llu splats, row %u B, tag %s, load %.1f ms\n", file.constData(),
      (unsigned long long)l.cloud->primitive_count, l.cloud->row_stride, tag.c_str(), l.ms);
  const int repeats = qEnvironmentVariableIsSet("SPLAT_PERF_REPEATS")
                          ? qEnvironmentVariableIntValue("SPLAT_PERF_REPEATS")
                          : 2;
  std::vector<ReadbackImage> images;
  for(int rep = 0; rep < repeats; ++rep)
  for(const auto& c : configs)
  {
    const auto scene = c.columns ? l.scene : withoutColumns(l);
    const QString decoder = QString::fromUtf8(c.decoder).startsWith('/')
                                ? QString::fromUtf8(c.decoder)
                                : corpus(c.decoder);
    const auto r = runChain(api, {scene, tag, decoder, 8, frames, {edge, edge}});
    if(r.skipped)
      SKIP(r.skip);
    INFO(r.error);
    REQUIRE(r.error.empty());
    std::printf(
        "[splat-perf] %s | %-40s | first %d frames %.1f ms | frame median %.3f min %.3f ms | rows up %llu (%llu B) | tables %llu "
        "(%llu B) | CSF owned %d (%lld B) | data=raw_splats %d | vk allocs %u used %llu B\n",
        r.backend.c_str(), c.name, 8, r.warmMs, r.frameMs, r.frameMinMs, (unsigned long long)r.rowUploads,
        (unsigned long long)r.rowBytes, (unsigned long long)r.tables,
        (unsigned long long)r.tableBytes, r.csfOwnedBuffers, (long long)r.csfOwnedBytes,
        int(r.columnData && r.columnData == r.rawSplats), r.allocCount,
        (unsigned long long)r.usedBytes);
    if(rep == 0)
      images.push_back(r.img);
  }
  if(tag == "3dgs.classic" && images.size() >= 2)
  {
    const auto d = compare(images[0], images[1]);
    std::printf(
        "[splat-perf] struct decode, column list vs none: max %d mean %.4f\n", d.maxAbs,
        d.meanAbs);
    CHECK(d.maxAbs == 0);
    const auto e = compare(images[0], images.back());
    std::printf("[splat-perf] struct vs by name: max %d mean %.4f\n", e.maxAbs, e.meanAbs);
  }
}
