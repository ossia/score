// A Voxel Loader mesh shows its outside.
//
// ogt_voxel_meshify winds every face counter-clockwise seen from outside the
// solid. The loader declares back-face culling with the front face its
// geometry carries, and a raw raster without a PIPELINE_STATE (the library's
// VoxelMeshRenderer) draws with exactly that. The raster below colours each
// fragment by its face normal and looks at a 2x2x2 solid from +Z: the centre of
// the frame is the +Z face when the front faces are kept, and the -Z face seen
// through the culled near side when they are not.
#include "GfxHalpNodes.hpp"

#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/VoxelLoader.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdint>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QLatin1String(file);
}

// MagicaVoxel file holding one solid 2x2x2 model.
QByteArray solidVox()
{
  auto u32 = [](std::uint32_t v) {
    return QByteArray(reinterpret_cast<const char*>(&v), 4);
  };
  auto chunk = [&](const char* id, const QByteArray& content,
                   const QByteArray& children = {}) -> QByteArray {
    return QByteArray(id, 4) + u32(content.size()) + u32(children.size()) + content
           + children;
  };
  QByteArray xyzi = u32(8);
  for(char z = 0; z < 2; z++)
    for(char y = 0; y < 2; y++)
      for(char x = 0; x < 2; x++)
        xyzi += QByteArray{} + x + y + z + char(79);
  const QByteArray model
      = chunk("SIZE", u32(2) + u32(2) + u32(2)) + chunk("XYZI", xyzi);
  return QByteArray("VOX ", 4) + u32(150) + chunk("MAIN", {}, model);
}

struct Shot
{
  bool skipped{};
  std::string error;
  ReadbackImage img;
};

Shot render(score::gfx::GraphicsApi api, int mode, const QString& voxPath)
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
    const int loader = p.addNode(procs.make<Threedim::VoxelLoader>(doc->context()));
    const int raster
        = p.addRaster(corpus("syn-voxel-facing.vs"), corpus("syn-voxel-facing.fs"));
    if(loader < 0 || raster < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(loader, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({96, 96});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    setInputs(
        *p.node(loader), {ossia::value{voxPath.toStdString()}, ossia::value{mode}});
    p.render(4);
    s.img = p.readback(sink);
    if(!s.img.valid())
      s.error = "empty readback";
  });
  return s;
}
}

TEST_CASE(
    "a Voxel Loader mesh shows the faces that point at the camera",
    "[gfx][threedim][voxel][cull]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  // VoxelLoader::VoxelMode: Mesh_Simple, Mesh_Greedy.
  const int mode = GENERATE(1, 2);
  CAPTURE(backend_name(api), mode);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vox = dir.filePath(QStringLiteral("solid.vox"));
  {
    QFile f(vox);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(solidVox());
  }

  const Shot s = render(api, mode, vox);
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());

  // n * 0.5 + 0.5: the +Z face is (128, 128, 255), the -Z face (128, 128, 0).
  const auto c = s.img.center();
  INFO("centre " << int(c[0]) << " " << int(c[1]) << " " << int(c[2]));
  CHECK(c[0] > 100);
  CHECK(c[0] < 156);
  CHECK(c[1] > 100);
  CHECK(c[1] < 156);
  CHECK(c[2] > 230);
}
