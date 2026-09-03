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
//   cube .stl (ascii+binary) / cube .ply (ascii) / cube .off (ascii)
//                   the SAME cube in the other containers -- the equal-geometry
//                   evidence for the pins and for the family oracle below
//   tiny.vox        a MagicaVoxel 2x2x2 solid (case currently SKIPs, see it)
// Real-world third-party samples are deliberately NOT committed; see
// threedim-render/fetch-real-assets.sh for the out-of-repo corpus.
//
// FORMERLY-PINNED DEFECTS, now fixed and asserting correct behaviour:
//   * obj-no-normals: GeometryLoader left an OBJ without `vn` unshaded so it
//     rendered BLACK under the Light projection; the loader now derives flat
//     per-face normals (the treatment STL got in 1a02c5cabf).
//   * stl-cube / ply-cube: the VCG import family (normals, no UVs) selected
//     the triplanar shader, which emitted only a texture and rendered black
//     untextured; the triplanar pass grew a normal-lighting floor.
//
// Cases found un-goldenable and asserted structurally instead:
//   * csf-geometry: csf-vertex-count-expr.cs is time-animated by design, so
//     two grabs never agree; asserted non-blank + blue-dominant (the raster's
//     particle colour), which a missing geometry cable turns black.
//
// Vulkan note: this suite pins the GL class only, but the model pipeline no
// longer ABORTS on the Vulkan backend. It used to hit a qrhivulkan.cpp assert
// ("utexD->m_flags.testFlag(QRhiTexture::UsedWithGenerateMips)" — generateMips
// requested on an input texture created without the flag); ModelDisplayNode
// now guards the generateMips call on the texture's flag, so a debug Qt
// Vulkan build renders the model pipeline instead of aborting.
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

#include <algorithm>
#include <cmath>
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

// ASCII OFF (Object File Format): the SAME cube again, in the third VCG
// container. Plain "OFF" carries POSITIONS ONLY -- no normals, no UVs -- so the
// mesh reaches ModelDisplay only if GeometryLoader's deriveMissingNormals
// synthesizes flat per-face normals for it, exactly as it does for an OBJ with
// no `vn`. The vertex layout is the PLY one (4 unshared corner-vertices per
// face, two triangles each) so that "derived" and "authored" normals are the
// same vectors and the four containers are pixel-comparable.
//
// This also stays on the plain-"OFF" branch of VcgImporters' offStructureIsSane
// pre-validation (2293b9d588): NOFF/COFF variants pass straight through to
// vcglib, so a variant header would silently stop testing the guarded path.
QByteArray makeCubeOffAscii()
{
  QByteArray verts;
  QByteArray faces;
  int vcount = 0, nfaces = 0;
  for(int f = 0; f < 6; f++)
  {
    const int* q = kCubeF[f];
    const int base = vcount;
    for(int k = 0; k < 4; k++)
    {
      const V3 v = kCubeV[q[k] - 1];
      verts += QStringLiteral("%1 %2 %3\n").arg(v.x).arg(v.y).arg(v.z).toUtf8();
      vcount++;
    }
    faces += QStringLiteral("3 %1 %2 %3\n").arg(base).arg(base + 1).arg(base + 2).toUtf8();
    faces += QStringLiteral("3 %1 %2 %3\n").arg(base).arg(base + 2).arg(base + 3).toUtf8();
    nfaces += 2;
  }
  QByteArray hdr = "OFF\n";
  hdr += QByteArray::number(vcount) + " " + QByteArray::number(nfaces) + " 0\n";
  return hdr + verts + faces;
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

//! The same asset->ModelDisplay->Window pipeline, but with the camera framed
//! and the `Camera` projection combo (inlet 9) driven.
//!
//! Two things the rest of this file does not do:
//!  * inlet 4 (FOV) is opened wide, because every fisheye law normalises to
//!    r=1 at the half-FOV and they only separate over a WIDE angular field;
//!  * inlets 2/3 (Position/Center) are written as PLAIN JS ARRAYS. That is the
//!    `setValue(QObject*, QList<qreal>)` overload (EditContext.port.cpp:376-389),
//!    and it works. `Qt.vector3d(x,y,z)` does NOT -- see the .vox case below for
//!    the root cause -- and the two are easy to confuse because the broken one
//!    is the one that looks like Qt.
QString fisheyeScene(
    const QString& assetPath, int cameraMode, double fovDeg, double eye,
    double centre)
{
  return QStringLiteral(R"JS(
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31";
Score.createDevice("Window", UUID_WINDOW, {});
var s = Score.find("Scenario.1"); if (s) Score.remove(s);
var root = Score.rootInterval();
var loader = Score.createProcess(root, "5df71765-505f-4ab7-98c1-f305d10a01ef", "%1");
if (!loader) { console.log("SCENE-ERROR: no loader"); Qt.exit(9); }
var md = Score.createProcess(root, "9ce44e4b-eeb6-4042-bb7f-9d0b28190daf", "");
if (!md) { console.log("SCENE-ERROR: no model display"); Qt.exit(9); }
Score.createCable(Score.outlet(loader, 0), Score.inlet(md, 1));
Score.setValue(Score.inlet(md, 2), [%4, %4, %4]);
Score.setValue(Score.inlet(md, 3), [%5, %5, %5]);
Score.setValue(Score.inlet(md, 4), %3);
Score.setValue(Score.inlet(md, 7), 6);
Score.setValue(Score.inlet(md, 8), 0);
Score.setValue(Score.inlet(md, 9), %2);
Score.setAddress(Score.outlet(md, 0), "Window:/");
Score.play();
)JS")
      .arg(assetPath)
      .arg(cameraMode)
      .arg(fovDeg)
      .arg(eye)
      .arg(centre);
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

//! Largest distance, in pixels, from the frame centre to a drawn pixel.
//!
//! For the fisheye cases this is a closed-form quantity and not a heuristic:
//! the drawn silhouette of a convex mesh is the convex hull of its projected
//! vertices, the farthest point of a convex polygon from any interior point is
//! a VERTEX, and all four fisheye laws are strictly increasing in the view
//! angle theta -- so the pixel that attains this maximum is the projection of
//! the same cube corner (the one at maximum theta) under every law. That is
//! what makes the ratios between the four renders a property of the LAWS alone,
//! independent of viewport aspect handling, of which corner it happens to be,
//! and of the absolute scale.
//!
//! Number of pixels above the `lit` threshold.
//!
//! nonBlank() is a MEAN over the whole frame, so it cannot be used as the
//! "something was drawn" floor for the fisheye cases: under the perspective law
//! at a 160-degree FOV the cube is legitimately a few percent of the frame and
//! its mean luma is well under the BLANK_MEAN rule. Measured -- that is what
//! this counter replaced.
int drawnPixels(const QImage& im, int lit = 24)
{
  int n = 0;
  for(int y = 0; y < im.height(); y++)
  {
    const uchar* row = im.constScanLine(y);
    for(int x = 0; x < im.width(); x++)
      if((int(row[x * 3]) + int(row[x * 3 + 1]) + int(row[x * 3 + 2])) / 3 > lit)
        n++;
  }
  return n;
}

//! `lit` is a luma threshold; the clear colour here is black.
double maxDrawnRadius(const QImage& im, int lit = 24)
{
  const double cx = (im.width() - 1) * 0.5;
  const double cy = (im.height() - 1) * 0.5;
  double best = -1.0;
  for(int y = 0; y < im.height(); y++)
  {
    const uchar* row = im.constScanLine(y);
    for(int x = 0; x < im.width(); x++)
    {
      const int l = (int(row[x * 3]) + int(row[x * 3 + 1]) + int(row[x * 3 + 2])) / 3;
      if(l <= lit)
        continue;
      const double dx = x - cx, dy = y - cy;
      best = std::max(best, std::sqrt(dx * dx + dy * dy));
    }
  }
  return best;
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

// GeometryLoader now derives flat per-face normals for any triangle mesh a
// loader returned without them (deriveMissingNormals), so an OBJ carrying no
// `vn` records is shaded by the Light projection instead of rendering black —
// the same visibility STL got from its per-face normals in 1a02c5cabf.
TEST_CASE(
    "an OBJ without normals must still be visible",
    "[integration][threedim][render][gui]")
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

// (Nomenclature correction, measured while adding the OFF case below: the VCG
// family is STL and OFF. PLY does NOT go through vcglib — GeometryLoader.cpp:290
// routes .ply to Threedim::PlyFromFile, the miniply reader in Ply.cpp. The two
// paths share the no-UV property that selects the triplanar shader, which is
// why they were grouped, but they are different importers and they do not
// render the same picture.)
//
// The no-UV loaders carry normals but no UVs, so under the
// Light projection they selected the triplanar shader — which emitted ONLY the
// projected texture and, with no texture wired, rendered pure black. The
// triplanar pass now has a normal-lighting floor (a wired texture still
// dominates via max()), so a plain STL/PLY cube is visible like the OBJ twin.
TEST_CASE(
    "an STL cube must render like the same cube as OBJ",
    "[integration][threedim][render][gui]")
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

// Same path as the STL case above: PLY (ascii, positions+normals, no UVs)
// went through the triplanar shader and rendered black without a texture;
// the triplanar lighting floor makes the untextured cube visible.
TEST_CASE(
    "a PLY cube must render like the same cube as OBJ",
    "[integration][threedim][render][gui]")
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

// -----------------------------------------------------------------------------
// P2-4 -- the container family, judged against each other rather than against a
// golden.
//
// The three cases above each assert only `nonBlank`, which is a coverage floor,
// not an oracle: a cube rendered at the wrong scale, mirrored, or shaded by the
// wrong normals is just as non-blank as the right one. What can pin these
// loaders instead is that they are four containers of ONE cube -- same eight
// corners, same winding, same six face normals -- so containers that reach the
// renderer with the same ATTRIBUTES must produce the same picture.
//
// Which containers those are was MEASURED here rather than assumed, and the
// spec's phrasing ("same cube as OBJ") turned out to be the wrong reference:
//
//   fam-stl vs fam-obj  meanAbs 15.03  fracFar 0.509
//   fam-off vs fam-obj  meanAbs 15.03  fracFar 0.509
//   fam-ply vs fam-obj  meanAbs 13.71  fracFar 0.377
//
// Half the frame differs, and that is BY DESIGN: the OBJ carries UVs and the
// VCG family does not, so they select different material paths -- the same fact
// the STL/PLY cases above already document. Gating on OBJ would have been a
// tolerance argument about two deliberately different pictures.
//
// The oracle that survives measurement is OFF vs STL. Both are VCG-family, both
// UV-less, both reach the material with positions plus one normal per face --
// STL's read from the file's facet records, OFF's SYNTHESIZED by
// GeometryLoader's deriveMissingNormals, since plain OFF carries no normals at
// all. Two loaders, two files, one picture. If the derivation regresses, OFF
// moves and STL does not.
//
// PLY is measured and REPORTED, not gated: it carries the same six normals
// per-vertex on the same 24-corner topology as OFF, and renders differently from
// both. Recorded as an open question rather than pinned -- see the ledger.
//
// Golden-free on purpose (SPEC §3.0): nothing is blessed, every leg is produced
// in the same run on the same GPU and driver, and the renderer line is compared
// across legs rather than assumed. The tolerance is the golden comparator's
// (meanAbs < 4, fracFar < 0.02).
//
// Cost: four app launches, ~15 s each, serialized on /tmp/score-harness.lock
// like every other case in this file.
TEST_CASE(
    "OBJ, STL, PLY and OFF of one cube render the same picture",
    "[integration][threedim][render][gui]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  auto write = [&](const char* file, const QByteArray& bytes) {
    const QString path = dir.filePath(QString::fromUtf8(file));
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly));
    REQUIRE(f.write(bytes) == bytes.size());
    f.close();
    return path;
  };

  auto render = [&](const char* name, const QString& asset) {
    return renderScene(
        dir, QString::fromUtf8(name), loaderScene(kGeometryLoader, asset, kProjLight));
  };

  // The reference leg: the container that has always worked, in this run.
  const auto ref = render("fam-obj", write("fam-cube.obj", makeCubeObj(true)));
  if(!ref.error.isEmpty())
    SKIP(ref.error.toStdString());
  if(!ref.rendererLine.contains("NVIDIA"))
    SKIP("renderer is not the nvidia-gl ref class: "
         << ref.rendererLine.toStdString());
  REQUIRE(nonBlank(ref.frame));

  struct Member
  {
    const char* name;
    QString asset;
    QImage frame;
  };
  Member members[]{
      {"fam-stl", write("fam-cube.stl", makeCubeStlAscii()), {}},
      {"fam-ply", write("fam-cube.ply", makeCubePlyAscii()), {}},
      {"fam-off", write("fam-cube.off", makeCubeOffAscii()), {}}};

  for(auto& m : members)
  {
    const auto r = render(m.name, m.asset);
    INFO(m.name << ": " << r.error.toStdString());
    REQUIRE(r.error.isEmpty());
    // Same GPU, same run: a renderer-class change mid-case would invalidate the
    // comparison, so it is checked rather than assumed.
    REQUIRE(r.rendererLine == ref.rendererLine);

    // Floor first, so a black frame names itself instead of surfacing as a
    // large diff of unclear origin.
    CHECK(nonBlank(r.frame));
    m.frame = r.frame;

    // Reported, never gated: the OBJ leg is NOT expected to match. Kept as a
    // measurement so the size of the UV-vs-no-UV shading difference is on the
    // record and a change in it is visible in the log.
    const Diff d = diffImages(m.frame, ref.frame);
    INFO(m.name << " vs fam-obj (reported, not gated): meanAbs=" << d.meanAbs
                << " fracFar=" << d.fracFar);
  }

  // The oracle: STL and OFF are the same cube with the same attributes.
  //
  // STL carries a normal per facet; plain OFF carries none and GeometryLoader
  // derives one per face. For a flat-shaded cube those are the SAME six vectors,
  // and neither container has UVs, so both take the triplanar path with the same
  // inputs. Two independent loaders (vcglib's STL reader and its OFF reader),
  // two different files on disk, one picture. That is the assertion.
  {
    const Diff d = diffImages(members[2].frame, members[0].frame);
    INFO("fam-off vs fam-stl: meanAbs=" << d.meanAbs << " fracFar=" << d.fracFar);
    CHECK(d.meanAbs < 4.0);
    CHECK(d.fracFar < 0.02);
  }

  // PLY is measured against the same reference and REPORTED, not gated. It
  // carries the same six normals STL does, written per-vertex in the file, on
  // the same 24-corner topology as OFF -- and it does not render the same
  // picture as either. Recorded rather than pinned: which of the two is correct
  // is a product question (see the ledger), and gating on the current value
  // would pin whichever answer today's code happens to give.
  {
    const Diff d = diffImages(members[1].frame, members[0].frame);
    INFO("fam-ply vs fam-stl (reported, not gated): meanAbs="
         << d.meanAbs << " fracFar=" << d.fracFar);
    WARN("PLY vs STL of the same cube: meanAbs=" << d.meanAbs << " fracFar="
                                                 << d.fracFar);
  }
}

TEST_CASE(
    "an OFF cube must render like the same cube as OBJ",
    "[integration][threedim][render][gui]")
{
  // The per-container floor, matching the STL and PLY cases above: OFF reaches
  // the model pipeline at all. The picture-identity claim is the family case.
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString off = dir.filePath("cube.off");
  {
    QFile f(off);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(makeCubeOffAscii());
  }
  const auto r
      = renderScene(dir, "off-cube", loaderScene(kGeometryLoader, off, kProjLight));
  if(!r.error.isEmpty())
    SKIP(r.error.toStdString());
  if(!r.rendererLine.contains("NVIDIA"))
    SKIP("renderer is not the nvidia-gl ref class");
  CHECK(nonBlank(r.frame));
}

// =============================================================================
// P2-3 -- the four fulldome projections.
//
// tests/threedim/FisheyeProjections.cpp guards the four GLSL snippets as SOURCE
// TEXT. That catches a fov/2-vs-fov/4 mix-up in the shipped strings and nothing
// else: it cannot tell whether the strings are compiled, whether the `Camera`
// combo (ModelDisplay inlet 9) reaches them, or whether the four laws are
// distinguishable in a frame. These two cases render them.
//
// THE ORACLE. Each law normalises to r=1 at the half-FOV h = fov/2 (q = fov/4),
// so with t the view angle of a point from the dome forward axis:
//
//   equidistant    r(t) = t        / h
//   equisolid      r(t) = sin(t/2) / sin(q)
//   stereographic  r(t) = tan(t/2) / tan(q)
//   orthographic   r(t) = sin(t)   / sin(h)
//
// The measured quantity is maxDrawnRadius() -- the farthest drawn pixel from the
// frame centre. It is closed-form and not a heuristic, for three reasons that
// have to hold together: the fisheye laws are applied PER VERTEX, so the
// rasterised silhouette is the convex hull of the projected corners; the
// farthest point of a convex polygon from an interior point is a vertex; and
// every law is strictly increasing in t, so the SAME corner -- the one at
// maximum t -- attains the maximum under all four.
//
// t_max is COMPUTED from the scene (eight known corners, a known eye, a known
// target), not fitted, so the four predictions share exactly ONE free parameter:
// the pixels-per-NDC scale along that corner's direction, fitted from the
// equidistant leg. The other three are predictions, and because the scale
// cancels in the ratio they are independent of viewport aspect handling and of
// absolute framing. Measured on this host, they land within 0.2%:
//
//   scale 581.26 px/NDC       predicted px   measured px   rel
//   equidistant  0.42055        244.45         244.45      (fitted)
//   equisolid    0.45023        261.70         261.21      0.0019
//   stereographic 0.36031       209.44         209.67      0.0011
//   orthographic 0.56258        327.01         326.36      0.0020
//
// The strict ordering asserted alongside is a theorem about the laws, not a
// fitted fact: sin is concave and tan convex on (0, pi/2) and each law is
// normalised at h, so for every t in (0,h)
//     r_ortho > r_equisolid > r_equidistant > r_stereographic.
// Nothing in the scene can reorder them, so two modes that render the SAME
// picture -- a combo that never reaches the shader -- cannot satisfy it.
//
// FOV is opened to 160 deg because all four laws agree to first order near the
// axis; at the default 35 deg they sit within a couple of percent of each other
// and no measurement separates them.
//
// -----------------------------------------------------------------------------
// WHY THERE ARE TWO CASES: the laws are right and the axis is wrong.
//
// The first case below is GREEN and the second is an [!shouldfail] pin, and the
// only difference between them is which way the camera points.
//
// ModelDisplayNode.cpp's four snippets all take the dome forward axis to be
// view-space +Z:
//     float theta = acos(clamp(d.z / r, -1.0, 1.0));
// with d = (matrixModelView * position).xyz. score's view matrix is the usual
// right-handed one -- it looks down view-space MINUS Z, which is what the
// Perspective mode in the same file projects along. So a model IN FRONT of the
// camera has d.z < 0, t comes out near pi, r_ndc = t/h is ~2.25 at a 160-degree
// FOV, and every vertex lands outside the clip box. The four fulldome modes
// image the hemisphere BEHIND the camera.
//
// Measured, 1280x720, cube at the origin, FOV 160:
//
//   eye (-0.7,-0.7,-0.7) -> centre (0,0,0)      [looking AT the cube]
//     Perspective    684 px drawn
//     equidistant      0 px      equisolid       0 px
//     stereographic    0 px      orthographic    0 px
//   eye (+0.7,+0.7,+0.7) -> centre (0,0,0)      [other side, still AT it]
//     equidistant      0 px
//   eye (-0.7,-0.7,-0.7) -> centre (-1.4,-1.4,-1.4)   [looking AWAY]
//     equidistant  16345 px      equisolid   18858 px
//     stereographic 11721 px     orthographic 29847 px
//
// So the maths in all four snippets is correct -- that is what the first case
// proves, to 0.2% -- and the axis they measure it from is not, which is what the
// second case pins. The fix is a sign, but WHICH sign is a product decision the
// header comment does not settle: the snippets say ".xzy re-orients world +Z as
// dome-up and world +Y as dome-forward; the view matrix then places the zenith
// along view-space +Z", i.e. they may be written for a dome rig whose camera is
// authored differently from the Position/Center pair the process actually
// exposes. Reported, not fixed.
//
// NEGATIVE CONTROL (run, see the ledger): the spec's own -- swap equidistant and
// equisolid in ModelDisplayNode.cpp's projections[].
// =============================================================================

namespace
{
constexpr double kFisheyeFovDeg = 160.0;
constexpr double kFisheyeEye = -0.7;   // camera at (-0.7,-0.7,-0.7)
constexpr double kFisheyePi = 3.14159265358979323846;

struct FisheyeLaw
{
  int mode;
  const char* name;
  double r; // closed-form NDC radius at t_max
};

//! t_max: the largest view angle of any cube corner from the dome forward axis,
//! which is -(centre - eye) normalised. Computed from the scene, never fitted.
double fisheyeTmax(double eye, double centre)
{
  const double lx = centre - eye;
  const double len = std::sqrt(3.0 * lx * lx);
  if(len <= 0)
    return 0.0;
  const double f = -lx / len; // view +Z, all three components equal
  double tmax = 0.0;
  for(const auto& c : kCubeV)
  {
    const double vx = c.x - eye, vy = c.y - eye, vz = c.z - eye;
    const double vl = std::sqrt(vx * vx + vy * vy + vz * vz);
    if(vl <= 0)
      continue;
    const double ct = std::clamp((vx * f + vy * f + vz * f) / vl, -1.0, 1.0);
    tmax = std::max(tmax, std::acos(ct));
  }
  return tmax;
}

std::array<FisheyeLaw, 4> fisheyeLaws(double tmax)
{
  const double h = (kFisheyeFovDeg * 0.5) * kFisheyePi / 180.0;
  const double q = (kFisheyeFovDeg * 0.25) * kFisheyePi / 180.0;
  return {FisheyeLaw{1, "equidistant", tmax / h},
          FisheyeLaw{2, "equisolid", std::sin(tmax * 0.5) / std::sin(q)},
          FisheyeLaw{3, "stereographic", std::tan(tmax * 0.5) / std::tan(q)},
          FisheyeLaw{4, "orthographic", std::sin(tmax) / std::sin(h)}};
}
} // namespace

TEST_CASE(
    "the four fulldome projections are four different, predicted radial mappings",
    "[integration][threedim][render][gui]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString obj = dir.filePath("fisheye-cube.obj");
  {
    QFile f(obj);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(makeCubeObj(true));
  }

  // The camera points AWAY from the cube, because that is the hemisphere these
  // shaders image (see the banner). This case is about the four LAWS; the axis
  // is pinned by the case below.
  constexpr double kCentre = 2.0 * kFisheyeEye;
  const double tmax = fisheyeTmax(kFisheyeEye, kCentre);
  REQUIRE(tmax > 0.1);
  REQUIRE(tmax < (kFisheyeFovDeg * 0.5) * kFisheyePi / 180.0); // nothing clipped

  const auto laws = fisheyeLaws(tmax);

  // The ordering is a theorem about the laws. Asserted on the PREDICTIONS first,
  // so an error in the closed forms above surfaces here rather than as a
  // confusing pixel failure.
  REQUIRE(laws[3].r > laws[1].r); // ortho > equisolid
  REQUIRE(laws[1].r > laws[0].r); // equisolid > equidistant
  REQUIRE(laws[0].r > laws[2].r); // equidistant > stereographic

  double measured[5]{};
  QString refRenderer;
  for(const auto& L : laws)
  {
    const auto r = renderScene(
        dir, QStringLiteral("fisheye-%1").arg(L.name),
        fisheyeScene(obj, L.mode, kFisheyeFovDeg, kFisheyeEye, kCentre));
    if(!r.error.isEmpty())
      SKIP(r.error.toStdString());
    if(!r.rendererLine.contains("NVIDIA"))
      SKIP("renderer is not the nvidia-gl ref class: "
           << r.rendererLine.toStdString());
    if(refRenderer.isEmpty())
      refRenderer = r.rendererLine;
    REQUIRE(r.rendererLine == refRenderer);

    const int drawn = drawnPixels(r.frame);
    const double rad = maxDrawnRadius(r.frame);
    INFO(L.name << ": mode " << L.mode << ", frame " << r.frame.width() << "x"
                << r.frame.height() << ", drawn " << drawn << " px, radius "
                << rad);
    REQUIRE(drawn > 64);
    REQUIRE(rad > 0.0);
    measured[L.mode] = rad;
  }

  INFO("tmax = " << tmax * 180.0 / kFisheyePi
                 << " deg, fov = " << kFisheyeFovDeg
                 << " deg; measured radii px: equid=" << measured[1]
                 << " equis=" << measured[2] << " stereo=" << measured[3]
                 << " ortho=" << measured[4]);

  // (a) The ordering the laws force, with a margin: two modes that render the
  //     same picture cannot satisfy it.
  CHECK(measured[4] > measured[2] + 2.0);
  CHECK(measured[2] > measured[1] + 2.0);
  CHECK(measured[1] > measured[3] + 2.0);

  // (b) The closed forms. One free parameter -- the px-per-NDC scale -- fitted
  //     from the equidistant leg; the other three are predictions. Measured
  //     agreement is 0.2%, so 1% + 2px is a real gate and not a formality.
  const double scale = measured[1] / laws[0].r;
  for(const auto& L : laws)
  {
    if(L.mode == 1)
      continue;
    const double predicted = scale * L.r;
    INFO(L.name << ": predicted " << predicted << " px, measured "
                << measured[L.mode] << " px");
    CHECK(std::abs(measured[L.mode] - predicted) <= 0.01 * predicted + 2.0);
  }
}

// EXPECTED TO FAIL -- [!shouldfail] pin. Asserts the CORRECT behaviour: a model
// in front of the camera is visible under every projection the Camera combo
// offers. Perspective draws it; all four fulldome modes draw NOTHING, because
// they take the dome forward axis to be view-space +Z while the view matrix
// looks down -Z. Goes green the day the axis is fixed. See the banner above.
TEST_CASE(
    "a model in front of the camera is visible under every Camera projection",
    "[integration][threedim][render][gui][!shouldfail]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString obj = dir.filePath("fisheye-front-cube.obj");
  {
    QFile f(obj);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(makeCubeObj(true));
  }

  // Camera looks AT the cube: eye (-0.7,-0.7,-0.7), centre the origin. This is
  // the rig the Perspective mode and every real Model Display uses.
  constexpr double kCentre = 0.0;

  // The control inside the pin: Perspective on this exact rig draws. Without it
  // a reader cannot tell "the fisheye modes are broken" from "the scene is
  // mis-framed", and the pin would be worth nothing.
  {
    const auto r = renderScene(
        dir, "fisheye-front-perspective",
        fisheyeScene(obj, 0, kFisheyeFovDeg, kFisheyeEye, kCentre));
    if(!r.error.isEmpty())
      SKIP(r.error.toStdString());
    if(!r.rendererLine.contains("NVIDIA"))
      SKIP("renderer is not the nvidia-gl ref class");
    INFO("perspective control: " << drawnPixels(r.frame) << " px drawn");
    REQUIRE(drawnPixels(r.frame) > 64);
  }

  for(const auto& L : fisheyeLaws(fisheyeTmax(kFisheyeEye, kCentre)))
  {
    const auto r = renderScene(
        dir, QStringLiteral("fisheye-front-%1").arg(L.name),
        fisheyeScene(obj, L.mode, kFisheyeFovDeg, kFisheyeEye, kCentre));
    if(!r.error.isEmpty())
      SKIP(r.error.toStdString());
    INFO(L.name << " (mode " << L.mode << "): " << drawnPixels(r.frame)
                << " px drawn");
    CHECK(drawnPixels(r.frame) > 64);
  }
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
