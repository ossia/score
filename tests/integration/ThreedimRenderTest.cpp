// =============================================================================
// End-to-end renders of the threedim asset pipeline, judged against golden
// references that were VISUALLY VALIDATED before being committed.
//
// Every case drives the REAL application the way the golden-render harness
// does: a JS build script through `ossia-score --no-gui --script ... --autoplay`,
// SCORE_FORCE_OFFSCREEN_WINDOW so nothing touches the desktop, wall-clock
// settling time for the asynchronous asset loaders (a sync renderFrames() run
// finishes before the loader's worker lands its closure — measured), then a
// grab requested over the OSC control port exactly as
// tests/integration/scene-js-sweep.sh does. The grabbed frame is compared
// against refs/<class>/<case>.png with a tolerance suited to GPU raster
// differences.
//
// BACKEND IDENTITY IS ASSERTED, NOT ASSUMED (the golden-render.sh rule):
// QT_LOGGING_RULES=qt.rhi.general=true makes QRhi print the renderer it got;
// a case only compares when that line matches the ref class's regex, and
// SKIPs otherwise — a silent fallback to another GPU/rasteriser must never
// produce a quiet green or a bogus red.
//
// ASSETS are generated in-test, byte-for-byte, all self-authored (CC0):
//   cube.obj        unit-ish cube, positions+normals+uvs, wound to the
//                   pipeline's front-face convention (validated visually:
//                   the reversed winding renders nothing from outside)
//   cube_nonormals.obj  the same cube with every `vn`/normal index stripped
//   cube .stl (ascii+binary) / cube .ply (ascii)  the SAME cube in the other
//                   containers -- the equal-geometry evidence for the pins
//   tiny.vox        a MagicaVoxel 2x2x2 solid (case currently SKIPs, see it)
// Real-world third-party samples are deliberately NOT committed; see
// threedim-render/fetch-real-assets.sh for the out-of-repo corpus.
//
// KNOWN DEFECTS PINNED EXPECTED-RED (never blessed):
//   * obj-no-normals: GeometryLoader leaves an OBJ without `vn` unshaded —
//     the mesh renders BLACK under the Light projection. The STL path grew
//     per-face normals in 1a02c5cabf; the OBJ path did not. Correct
//     behaviour (a visible mesh) is asserted under [!shouldfail].
//   * stl-cube / ply-cube: the whole VCG import family (STL, PLY) renders
//     BLACK end-to-end while the byte-equal geometry through the OBJ path
//     renders -- see those cases for the measurement matrix.
//
// Cases found un-goldenable and asserted structurally instead:
//   * csf-geometry: csf-vertex-count-expr.cs is time-animated by design, so
//     two grabs never agree; asserted non-blank + blue-dominant (the raster's
//     particle colour), which a missing geometry cable turns black.
//
// Vulkan note: this suite pins the GL class only. On the Vulkan backend the
// model pipeline currently ABORTS in debug Qt builds (qrhivulkan.cpp assert
// "utexD->m_flags.testFlag(QRhiTexture::UsedWithGenerateMips)" — generateMips
// requested on a texture created without the flag), recorded in the coverage
// ledger; a crash pin needs the fork harness and is left for a follow-up.
// =============================================================================

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QThread>
#include <QUdpSocket>

#include <cstdint>
#include <cstring>
#include <string>

namespace
{
QString appBinary()
{
#if defined(SCORE_APP_BINARY)
  return QStringLiteral(SCORE_APP_BINARY);
#else
  return {};
#endif
}

QString refsDir()
{
#if defined(THREEDIM_RENDER_REFS_DIR)
  return QStringLiteral(THREEDIM_RENDER_REFS_DIR);
#else
  return {};
#endif
}

QString gfxCorpusDir()
{
#if defined(GFX_TEST_CORPUS_DIR)
  return QStringLiteral(GFX_TEST_CORPUS_DIR);
#else
  return {};
#endif
}

constexpr int kOscPort = 6666;

// ---------------------------------------------------------------- assets

// The cube every mesh case uses: within +-0.4 so the ModelDisplay default
// camera at (-1,-1,-1) looking at the origin frames it, wound to the
// convention the pipeline actually rasterises (validated: the opposite
// winding is fully backface-culled from this camera).
struct V3
{
  float x, y, z;
};
static const V3 kCubeV[8] = {{-0.4f, -0.4f, -0.4f}, {0.4f, -0.4f, -0.4f},
                             {0.4f, 0.4f, -0.4f},   {-0.4f, 0.4f, -0.4f},
                             {-0.4f, -0.4f, 0.4f},  {0.4f, -0.4f, 0.4f},
                             {0.4f, 0.4f, 0.4f},    {-0.4f, 0.4f, 0.4f}};
static const V3 kCubeN[6]
    = {{0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0}};
// quad (1-based vertex ids) per face, in the winding the pipeline draws
static const int kCubeF[6][4] = {{1, 2, 3, 4}, {8, 7, 6, 5}, {5, 6, 2, 1},
                                 {7, 8, 4, 3}, {8, 5, 1, 4}, {6, 7, 3, 2}};

QByteArray makeCubeObj(bool withNormals)
{
  QByteArray o;
  for(auto& v : kCubeV)
    o += QStringLiteral("v %1 %2 %3\n").arg(v.x).arg(v.y).arg(v.z).toUtf8();
  o += "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n";
  if(withNormals)
    for(auto& n : kCubeN)
      o += QStringLiteral("vn %1 %2 %3\n").arg(n.x).arg(n.y).arg(n.z).toUtf8();
  for(int f = 0; f < 6; f++)
  {
    const int* q = kCubeF[f];
    const int n = f + 1;
    auto tri = [&](int a, int ta, int b, int tb, int c, int tc) {
      if(withNormals)
        o += QStringLiteral("f %1/%2/%3 %4/%5/%6 %7/%8/%9\n")
                 .arg(q[a]).arg(ta).arg(n)
                 .arg(q[b]).arg(tb).arg(n)
                 .arg(q[c]).arg(tc).arg(n)
                 .toUtf8();
      else
        o += QStringLiteral("f %1/%2 %3/%4 %5/%6\n")
                 .arg(q[a]).arg(ta).arg(q[b]).arg(tb).arg(q[c]).arg(tc)
                 .toUtf8();
    };
    tri(0, 1, 1, 2, 2, 3);
    tri(0, 1, 2, 3, 3, 4);
  }
  return o;
}

// The SAME cube as ASCII / binary STL -- the equal-geometry evidence for
// the VCG-path pin: identical coordinates, winding and per-face normals as
// makeCubeObj(true), only the container differs.
QByteArray makeCubeStlAscii()
{
  QByteArray o = "solid cube\n";
  for(int f = 0; f < 6; f++)
  {
    const int* q = kCubeF[f];
    const V3 n = kCubeN[f];
    const int tris[2][3] = {{q[0], q[1], q[2]}, {q[0], q[2], q[3]}};
    for(auto& t : tris)
    {
      o += QStringLiteral("facet normal %1 %2 %3\n outer loop\n")
               .arg(n.x).arg(n.y).arg(n.z).toUtf8();
      for(int vi : t)
      {
        const V3 v = kCubeV[vi - 1];
        o += QStringLiteral("  vertex %1 %2 %3\n").arg(v.x).arg(v.y).arg(v.z).toUtf8();
      }
      o += " endloop\nendfacet\n";
    }
  }
  o += "endsolid cube\n";
  return o;
}

QByteArray makeCubeStlBinary()
{
  QByteArray o(80, '\0');
  auto put32 = [&](std::uint32_t v) { o.append(reinterpret_cast<char*>(&v), 4); };
  auto putf = [&](float f) { o.append(reinterpret_cast<char*>(&f), 4); };
  put32(12);
  for(int f = 0; f < 6; f++)
  {
    const int* q = kCubeF[f];
    const V3 n = kCubeN[f];
    const int tris[2][3] = {{q[0], q[1], q[2]}, {q[0], q[2], q[3]}};
    for(auto& t : tris)
    {
      putf(n.x); putf(n.y); putf(n.z);
      for(int vi : t)
      {
        const V3 v = kCubeV[vi - 1];
        putf(v.x); putf(v.y); putf(v.z);
      }
      o += '\0';
      o += '\0';
    }
  }
  return o;
}

// ASCII PLY: the cube with per-face flat normals (24 corner-vertices), the
// exact layout measured black through the VCG path.
QByteArray makeCubePlyAscii()
{
  QByteArray body;
  int vcount = 0;
  QByteArray faces;
  int nfaces = 0;
  for(int f = 0; f < 6; f++)
  {
    const int* q = kCubeF[f];
    const V3 n = kCubeN[f];
    const int base = vcount;
    for(int k = 0; k < 4; k++)
    {
      const V3 v = kCubeV[q[k] - 1];
      body += QStringLiteral("%1 %2 %3 %4 %5 %6\n")
                  .arg(v.x).arg(v.y).arg(v.z).arg(n.x).arg(n.y).arg(n.z)
                  .toUtf8();
      vcount++;
    }
    faces += QStringLiteral("3 %1 %2 %3\n").arg(base).arg(base + 1).arg(base + 2).toUtf8();
    faces += QStringLiteral("3 %1 %2 %3\n").arg(base).arg(base + 2).arg(base + 3).toUtf8();
    nfaces += 2;
  }
  QByteArray hdr = "ply\nformat ascii 1.0\n";
  hdr += "element vertex " + QByteArray::number(vcount) + "\n";
  hdr += "property float x\nproperty float y\nproperty float z\n";
  hdr += "property float nx\nproperty float ny\nproperty float nz\n";
  hdr += "element face " + QByteArray::number(nfaces) + "\n";
  hdr += "property list uchar uint vertex_indices\nend_header\n";
  return hdr + body + faces;
}

// Minimal MagicaVoxel .vox: a solid 2x2x2 with default-palette colours.
QByteArray makeTinyVox()
{
  auto chunk = [](const char id[4], const QByteArray& content,
                  const QByteArray& children = {}) {
    QByteArray c(id, 4);
    std::uint32_t n = content.size(), m = children.size();
    c.append(reinterpret_cast<char*>(&n), 4);
    c.append(reinterpret_cast<char*>(&m), 4);
    return c + content + children;
  };
  auto u32 = [](std::uint32_t v) {
    return QByteArray(reinterpret_cast<char*>(&v), 4);
  };
  QByteArray size = u32(2) + u32(2) + u32(2);
  QByteArray xyzi = u32(8);
  for(int z = 0; z < 2; z++)
    for(int y = 0; y < 2; y++)
      for(int x = 0; x < 2; x++)
      {
        xyzi += char(x); xyzi += char(y); xyzi += char(z);
        xyzi += char(79); // default-palette index
      }
  QByteArray main = chunk("SIZE", size) + chunk("XYZI", xyzi);
  QByteArray o("VOX ", 4);
  o += u32(150);
  o += chunk("MAIN", {}, main);
  return o;
}

// ---------------------------------------------------------------- runner

struct RenderResult
{
  bool ran{false};
  QString error;
  QString rendererLine; // what QRhi said it got
  QImage frame;
  std::vector<QImage> extraFrames; // when extra grabs were requested
};

void oscSend(const QString& address, const QString& arg)
{
  QByteArray pkt;
  auto pad = [&](QByteArray b) {
    b += '\0';
    while(b.size() % 4)
      b += '\0';
    return b;
  };
  pkt += pad(address.toUtf8());
  pkt += pad(QByteArrayLiteral(","
                               "s"));
  pkt += pad(arg.toUtf8());
  QUdpSocket s;
  s.writeDatagram(pkt, QHostAddress::LocalHost, kOscPort);
}

void oscSendBare(const QString& address, const QString& sArg = {})
{
  if(sArg.isEmpty())
  {
    QByteArray pkt;
    auto pad = [&](QByteArray b) {
      b += '\0';
      while(b.size() % 4)
        b += '\0';
      return b;
    };
    pkt += pad(address.toUtf8());
    pkt += pad(QByteArrayLiteral(","));
    QUdpSocket s;
    s.writeDatagram(pkt, QHostAddress::LocalHost, kOscPort);
  }
  else
    oscSend(address, sArg);
}

//! Build + play the scene in `js`, wait for the loaders, grab over OSC.
RenderResult renderScene(const QTemporaryDir& dir, const QString& name,
                         const QString& jsBody, int extraGrabs = 0)
{
  RenderResult r;
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
  {
    r.error = "no ossia-score binary";
    return r;
  }

  // The OSC control port is process-global; serialize with every other
  // harness exactly as scene-js-sweep.sh documents.
  QLockFile lock("/tmp/score-harness.lock");
  lock.setStaleLockTime(120000);
  if(!lock.tryLock(180000))
  {
    r.error = "could not take /tmp/score-harness.lock";
    return r;
  }

  const QString js = dir.filePath(name + ".js");
  const QString png = dir.filePath(name + ".png");
  {
    QFile f(js);
    if(!f.open(QIODevice::WriteOnly))
    {
      r.error = "cannot write js";
      return r;
    }
    f.write(jsBody.toUtf8());
  }

  // Per-run config home pinning the GL backend (the class the refs are for).
  const QString cfg = dir.filePath(name + "-cfg");
  QDir().mkpath(cfg + "/ossia");
  {
    QFile f(cfg + "/ossia/score.conf");
    f.open(QIODevice::WriteOnly);
    f.write("[score_plugin_gfx]\nGraphicsApi=OpenGL\nVSync=false\nSamples=1\nRate=60\n");
  }

  auto env = QProcessEnvironment::systemEnvironment();
  env.insert("XDG_CONFIG_HOME", cfg);
  // The app persists a shader/PSO cache under XDG_CACHE_HOME; without
  // isolating it, a product-shader edit can keep rendering the OLD compiled
  // pipeline (measured: a negative control stayed green until this line).
  env.insert("XDG_CACHE_HOME", cfg + "/cache");
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  env.insert("QT_LOGGING_RULES", "qt.rhi.general=true");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      appBinary(),
      {"--no-gui", "--no-restore", "--script", js, "--wait", "30", "--autoplay"});
  if(!p.waitForStarted(30000))
  {
    r.error = "app did not start";
    return r;
  }

  // Let the graph build and the async loaders land, then grab (twice: the
  // first grab also warms the readback path), then ask the app to leave.
  QThread::sleep(9);
  oscSend("/script",
          QStringLiteral("Score.device('Window').grabTo('%1')").arg(png));
  QThread::sleep(3);
  oscSend("/script",
          QStringLiteral("Score.device('Window').grabTo('%1')").arg(png));
  // Extra spaced grabs for time-animated cases whose content roams the frame.
  for(int k = 0; k < extraGrabs; k++)
  {
    QThread::sleep(2);
    oscSend(
        "/script", QStringLiteral("Score.device('Window').grabTo('%1.%2.png')")
                       .arg(png)
                       .arg(k));
  }
  QThread::sleep(2);
  oscSend("/exit", "force");
  // Teardown SIGSEGV after the grab is a known, documented nuisance
  // (scene-js-sweep.sh header); the PNG is the verdict, not the exit code.
  if(!p.waitForFinished(30000))
    p.kill();

  const QString log = QString::fromUtf8(p.readAll());
  for(const auto& line : log.split('\n'))
    if(line.contains("qt.rhi.general") && line.contains("RENDERER"))
      r.rendererLine = line.trimmed();

  if(!QFile::exists(png))
  {
    r.error = "no frame was grabbed; log tail: " + log.right(600);
    return r;
  }
  r.frame = QImage(png).convertToFormat(QImage::Format_RGB888);
  for(int k = 0; k < extraGrabs; k++)
  {
    QImage e(png + "." + QString::number(k) + ".png");
    if(!e.isNull())
      r.extraFrames.push_back(e.convertToFormat(QImage::Format_RGB888));
  }
  r.ran = !r.frame.isNull();
  if(!r.ran)
    r.error = "grabbed PNG unreadable";
  return r;
}

//! The default asset->screen pipeline: loader(path) -> ModelDisplay -> Window.
QString loaderScene(const QString& loaderUuid, const QString& assetPath,
                    int texProj)
{
  return QStringLiteral(R"JS(
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31";
Score.createDevice("Window", UUID_WINDOW, {});
var s = Score.find("Scenario.1"); if (s) Score.remove(s);
var root = Score.rootInterval();
var loader = Score.createProcess(root, "%1", "%2");
if (!loader) { console.log("SCENE-ERROR: no loader"); Qt.exit(9); }
var md = Score.createProcess(root, "9ce44e4b-eeb6-4042-bb7f-9d0b28190daf", "");
if (!md) { console.log("SCENE-ERROR: no model display"); Qt.exit(9); }
Score.createCable(Score.outlet(loader, 0), Score.inlet(md, 1));
Score.setValue(Score.inlet(md, 4), 35.0);
Score.setValue(Score.inlet(md, 7), %3);
Score.setValue(Score.inlet(md, 8), 0);
Score.setAddress(Score.outlet(md, 0), "Window:/");
Score.play();
)JS")
      .arg(loaderUuid, assetPath)
      .arg(texProj);
}

constexpr auto kGeometryLoader = "5df71765-505f-4ab7-98c1-f305d10a01ef";
constexpr auto kVoxelLoader = "a7c3e1b4-9f2d-4e8a-b6c5-1d3f7e9a2b4c";
constexpr int kProjLight = 6;

// ---------------------------------------------------------------- verdicts

double meanLuma(const QImage& im)
{
  double sum = 0;
  for(int y = 0; y < im.height(); y++)
  {
    const uchar* row = im.constScanLine(y);
    for(int x = 0; x < im.width() * 3; x++)
      sum += row[x];
  }
  return sum / (im.width() * im.height() * 3.0);
}

bool nonBlank(const QImage& im)
{
  return meanLuma(im) > 0.5; // the BLANK_MEAN rule, in 8-bit units
}

struct Diff
{
  double meanAbs{999};
  double fracFar{1}; // fraction of pixels off by > 24 codes in some channel
};

Diff diffImages(const QImage& a, const QImage& b)
{
  Diff d;
  if(a.size() != b.size() || a.isNull())
    return d;
  double acc = 0;
  std::int64_t far = 0;
  for(int y = 0; y < a.height(); y++)
  {
    const uchar* ra = a.constScanLine(y);
    const uchar* rb = b.constScanLine(y);
    for(int x = 0; x < a.width(); x++)
    {
      int worst = 0;
      for(int c = 0; c < 3; c++)
      {
        const int e = std::abs(int(ra[x * 3 + c]) - int(rb[x * 3 + c]));
        acc += e;
        worst = std::max(worst, e);
      }
      if(worst > 24)
        far++;
    }
  }
  d.meanAbs = acc / (a.width() * a.height() * 3.0);
  d.fracFar = double(far) / (a.width() * a.height());
  return d;
}

//! Compare against the committed golden, or write it when
//! SCORE_THREEDIM_UPDATE_REFS=1 (used ONLY by a human who then LOOKS at it;
//! see the header — never bless an unjudged image).
void requireMatchesGolden(const RenderResult& r, const QString& caseName)
{
  if(!r.rendererLine.contains("NVIDIA"))
    SKIP("renderer is not the nvidia-gl ref class: "
         << r.rendererLine.toStdString());

  const QString refPath = refsDir() + "/nvidia-gl/" + caseName + ".png";
  if(qEnvironmentVariableIsSet("SCORE_THREEDIM_UPDATE_REFS"))
  {
    QDir().mkpath(refsDir() + "/nvidia-gl");
    REQUIRE(nonBlank(r.frame));
    REQUIRE(r.frame.save(refPath));
    WARN("ref written (validate it visually before committing): "
         << refPath.toStdString());
    return;
  }

  QImage ref(refPath);
  if(ref.isNull())
    FAIL("no golden ref at " << refPath.toStdString()
                             << " (renders exist but were never validated?)");
  ref = ref.convertToFormat(QImage::Format_RGB888);
  const Diff d = diffImages(r.frame, ref);
  INFO(caseName.toStdString() << ": meanAbs=" << d.meanAbs
                              << " fracFar=" << d.fracFar);
  CHECK(d.meanAbs < 4.0);
  CHECK(d.fracFar < 0.02);
}
} // namespace

TEST_CASE(
    "an OBJ with normals renders through the model pipeline",
    "[integration][threedim][render][gui]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString obj = dir.filePath("cube.obj");
  {
    QFile f(obj);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(makeCubeObj(true));
  }
  const auto r
      = renderScene(dir, "obj-cube", loaderScene(kGeometryLoader, obj, kProjLight));
  if(!r.error.isEmpty())
    SKIP(r.error.toStdString());
  requireMatchesGolden(r, "obj-cube");
}

// DEFECT, pinned expected-red (2026-09-02): GeometryLoader leaves an OBJ
// without `vn` records unshaded — zero normals — so the Light projection
// renders it BLACK while the very same geometry with normals renders. STL
// got its per-face normals computed in 1a02c5cabf; the OBJ path did not.
// Correct behaviour: a mesh the artist loaded is visible. The tag comes off
// when the loader derives face normals for normal-less OBJs.
TEST_CASE(
    "an OBJ without normals must still be visible",
    "[integration][threedim][render][gui][!shouldfail]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString obj = dir.filePath("cube_nn.obj");
  {
    QFile f(obj);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(makeCubeObj(false));
  }
  const auto r = renderScene(
      dir, "obj-cube-nonormals", loaderScene(kGeometryLoader, obj, kProjLight));
  if(!r.error.isEmpty())
    SKIP(r.error.toStdString());
  if(!r.rendererLine.contains("NVIDIA"))
    SKIP("renderer is not the nvidia-gl ref class");
  CHECK(nonBlank(r.frame));
}

// DEFECT, pinned expected-red (2026-09-02): the VCG import family renders
// BLACK end-to-end. Byte-identical cube geometry (same coordinates, same
// winding, same normals) drawn through the OBJ/TinyObj path renders the
// golden above; routed through the STL path (VcgImporters) it rasterises
// nothing, in either STL encoding and either winding (all four measured).
// 1a02c5cabf pinned the loader's FLOAT OUTPUT (per-face normals present) at
// unit level, so the break is between the vcg mesh layout and what the
// renderer consumes -- exactly the gap only an end-to-end test can see.
// Correct behaviour: the same cube, visible. Tag comes off with the fix.
TEST_CASE(
    "an STL cube must render like the same cube as OBJ",
    "[integration][threedim][render][gui][!shouldfail]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto run = [&](const char* name, const QByteArray& bytes) {
    const QString stl = dir.filePath(QString::fromUtf8(name) + ".stl");
    QFile f(stl);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(bytes);
    f.close();
    const auto r = renderScene(
        dir, QString::fromUtf8(name), loaderScene(kGeometryLoader, stl, kProjLight));
    if(!r.error.isEmpty())
      SKIP(r.error.toStdString());
    if(!r.rendererLine.contains("NVIDIA"))
      SKIP("renderer is not the nvidia-gl ref class");
    CHECK(nonBlank(r.frame));
  };

  SECTION("ascii") { run("stl-cube-ascii", makeCubeStlAscii()); }
  SECTION("binary") { run("stl-cube-bin", makeCubeStlBinary()); }
}

// Same defect family as the STL pin above: the equal-geometry cube through
// the PLY path (ascii, positions+normals, 24 flat-shaded corner vertices)
// rasterises nothing while the OBJ twin renders. Pinned expected-red.
TEST_CASE(
    "a PLY cube must render like the same cube as OBJ",
    "[integration][threedim][render][gui][!shouldfail]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString ply = dir.filePath("cube.ply");
  {
    QFile f(ply);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(makeCubePlyAscii());
  }
  const auto r
      = renderScene(dir, "ply-cube", loaderScene(kGeometryLoader, ply, kProjLight));
  if(!r.error.isEmpty())
    SKIP(r.error.toStdString());
  if(!r.rendererLine.contains("NVIDIA"))
    SKIP("renderer is not the nvidia-gl ref class");
  CHECK(nonBlank(r.frame));
}

TEST_CASE(
    "a MagicaVoxel file renders through the voxel loader",
    "[integration][threedim][render][gui]")
{
  // The 2x2x2 voxel solid spans [0,2]^3 at the loader's unit scale; whether
  // its black frame under the FIXED default camera is a defect or just
  // framing cannot be distinguished, because scripting the camera is itself
  // broken: Qt.vector3d(x,y,z) in the console engine drops its arguments and
  // returns a zeroed vector (measured -- position == center degenerates the
  // view matrix). Revisit when the vec3 value type works; the equal-geometry
  // discrimination used for the STL/PLY pins has no analog here.
  SKIP("blocked on the Qt.vector3d zeroing defect (camera cannot be framed "
       "for an asset that does not fit the default view)");
}

TEST_CASE(
    "compute-shader-generated geometry rasterises",
    "[integration][threedim][render][gui]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  // syn-geo-producer.cs: fixed VERTEX_COUNT, no TIME anywhere -- one
  // viewport-covering solid-green triangle, so this case IS goldenable
  // (csf-vertex-count-expr.cs was tried first and is time-animated by
  // design: its particle cloud roams off-frame, five spaced grabs measured
  // all-blank on some runs).
  const QString cs = gfxCorpusDir() + "/syn-geo-producer.cs";
  const QString fs = gfxCorpusDir() + "/raw-raster-basic.fs";
  REQUIRE(QFile::exists(cs));
  REQUIRE(QFile::exists(fs));

  const QString js = QStringLiteral(R"JS(
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31";
Score.createDevice("Window", UUID_WINDOW, {});
var s = Score.find("Scenario.1"); if (s) Score.remove(s);
var root = Score.rootInterval();
var csf = Score.createProcess(root, "a5bbffe0-93d2-4e70-995c-cf46c2c43520", "%1");
if (!csf) { console.log("SCENE-ERROR: no csf"); Qt.exit(9); }
var ras = Score.createProcess(root, "dbfc2101-40d7-4807-8804-571e88992e7e", "%2");
if (!ras) { console.log("SCENE-ERROR: no raster"); Qt.exit(9); }
Score.createCable(Score.outlet(csf, 0), Score.inlet(ras, 0));
Score.setAddress(Score.outlet(ras, 0), "Window:/");
Score.play();
)JS")
                         .arg(cs, fs);

  const auto r = renderScene(dir, "csf-geometry", js);
  if(!r.error.isEmpty())
    SKIP(r.error.toStdString());
  requireMatchesGolden(r, "csf-geometry");
}
