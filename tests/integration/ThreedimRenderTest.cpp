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
// against refs/<case>.png -- ONE golden per case, shared by every backend --
// through tests/integration/golden-render/compare.py, which owns the only
// golden tolerance in the tree. See GoldenImage.hpp for why the comparison is
// not open-coded here any more.
//
// BACKEND IDENTITY IS ASSERTED, NOT ASSUMED (the golden-render.sh rule), AND
// NOTHING IN THIS FILE IS GATED ON IT.
//
// Every leg reports the device that produced it, read from score.gfx's own
// "RHI device:" line, which RenderState::Caps::populate prints from
// QRhi::backendName() and QRhi::driverInfo() -- available on every backend
// since Qt 6.4 -- so a number always names what produced it.
//
// It used to be read out of Qt's qt.rhi.general log by looking for the literal
// "RENDERER". That string is emitted by exactly ONE backend: qrhigles2.cpp's
// "OpenGL VENDOR: %s RENDERER: %s VERSION: %s". Vulkan prints "Using imported
// physical device '<name>' ... vendor 0x.. device 0x..", D3D11 and D3D12 print
// adapter lines of their own, and none of them contains the word. The scrape
// therefore returned an EMPTY string on every backend but OpenGL, and two
// things followed silently:
//
//   * nine `if(!rendererLine.contains("NVIDIA")) SKIP(...)` sites were
//     unconditionally true off OpenGL. Measured here on a machine with an
//     NVIDIA card in it: on QSG_RHI_BACKEND=vulkan the file skipped every one
//     of those cases anyway. It was not a hardware gate, it was a log-format
//     accident, and it cost a full render (~16 s a leg, 183 s a leg on the
//     Windows sweep) to reach a verdict of "skipped".
//   * the cross-leg "the same renderer produced both frames" checks compared
//     "" against "" and could never fire.
//
// Both are fixed. The nonBlank / ordering / closed-form assertions below are
// statements about loaders, shaders and projection arithmetic, not about
// drivers, and they now run on every backend and every vendor. The one thing
// that still skips does so on a CHECKED capability fact and not on a string:
// skipIfNothingIsRasterised() skips when QRhi::backendName() is "Null",
// because that backend records commands and rasterises nothing, so every pixel
// assertion in this file would be reading a cleared frame.
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
//     particle colour), which a missing geometry cable turns black. It now
//     renders syn-geo-asym-tri.cs instead -- a fixed, clock-free, deliberately
//     LOPSIDED triangle -- and is goldenable again; see the case.
//
// CHANNELS found un-goldenable, on an otherwise goldenable case:
//   * obj-cube's BLUE. The phong shader the Light projection selects animates
//     its own light off the transport clock, and its materials aim the whole
//     animation at the specular, which is blue-only. Measured over six renders
//     on one machine, R and G are bit-identical and B is not reproducible even
//     against itself. The golden covers "rg"; the specular is asserted by its
//     shape. Full derivation at the case.
//
// Vulkan note: this suite pins the GL class only, but the model pipeline no
// longer ABORTS on the Vulkan backend. It used to hit a qrhivulkan.cpp assert
// ("utexD->m_flags.testFlag(QRhiTexture::UsedWithGenerateMips)" — generateMips
// requested on an input texture created without the flag); ModelDisplayNode
// now guards the generateMips call on the texture's flag, so a debug Qt
// Vulkan build renders the model pipeline instead of aborting.
// =============================================================================

#include "GoldenImage.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
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

// The same cube with normals but NO texture coordinates: no `vt` records and
// `f v//vn` faces. TinyObj derives tangents only for a shape that has BOTH
// texcoords and normals (TinyObj.cpp:311-325), so dropping the UVs drops the
// tangents with them -- which is what makes this the no-UV AND no-tangent leg
// of the attribute matrix below.
QByteArray makeCubeObjNoUv()
{
  QByteArray o;
  for(auto& v : kCubeV)
    o += QStringLiteral("v %1 %2 %3\n").arg(v.x).arg(v.y).arg(v.z).toUtf8();
  for(auto& n : kCubeN)
    o += QStringLiteral("vn %1 %2 %3\n").arg(n.x).arg(n.y).arg(n.z).toUtf8();
  for(int f = 0; f < 6; f++)
  {
    const int* q = kCubeF[f];
    const int n = f + 1;
    auto tri = [&](int a, int b, int c) {
      o += QStringLiteral("f %1//%2 %3//%4 %5//%6\n")
               .arg(q[a]).arg(n).arg(q[b]).arg(n).arg(q[c]).arg(n)
               .toUtf8();
    };
    tri(0, 1, 2);
    tri(0, 2, 3);
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
  // -> QByteArray is load-bearing, not style. Without it `auto` deduces the
  // QStringBuilder<QByteArray&, const QByteArray&, ...> expression template
  // that `c + content + children` builds; it holds REFERENCES to all three
  // operands, every one of which dies at the return, and the caller converts a
  // dangling proxy. Measured: SIGABRT inside QByteArray's constructor on a
  // garbage length, the moment this function was first called for real. It
  // never had been -- the .vox case below SKIPped unconditionally, so the
  // fixture it exists to build had never once been executed.
  auto chunk = [](const char id[4], const QByteArray& content,
                  const QByteArray& children = {}) -> QByteArray {
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

//! Who rendered the frame, as QRhi itself reports it.
//!
//! Read out of score.gfx's own "RHI device:" line, which RenderState::Caps
//! ::populate prints from QRhi::backendName() + QRhi::driverInfo() on EVERY
//! backend. It replaces a scrape of Qt's qt.rhi.general log for the literal
//! "RENDERER", which only qrhigles2.cpp ever emits ("OpenGL VENDOR: %s
//! RENDERER: %s VERSION: %s"): Vulkan, D3D11 and D3D12 print adapter lines of
//! entirely different shapes, so that scrape returned an EMPTY string off
//! every backend but OpenGL. Two things fell out of that, both of them silent:
//! the vendor gates below were unconditionally true and skipped the whole file
//! on three backends regardless of the hardware, and the cross-leg
//! "same renderer" checks compared "" against "" and could never fire.
//!
//! Fields are populated per backend as the driver allows. deviceName is filled
//! in everywhere; vendorId/deviceId are zero on Qt's GL backend (measured here:
//! GL reports device="NVIDIA Corporation Quadro RTX 4000/PCIe/SSE2 4.6.0 NVIDIA
//! 595.84" vendorId=0x0, Vulkan on the same box reports device="NVIDIA GeForce
//! RTX 4090" vendorId=0x10de deviceId=0x2684) -- the same gap
//! GpuCapabilities.cpp:213 documents. Nothing in this file GATES on any of it.
struct DeviceIdentity
{
  QString line;       //!< the whole reported identity, verbatim
  QString backend;    //!< QRhi::backendName(): OpenGL/Vulkan/D3D11/D3D12/Metal/Null
  QString deviceName; //!< QRhiDriverInfo::deviceName
  QString deviceType; //!< integrated/discrete/cpu/virtual/external/unknown
  quint64 vendorId{0};
  quint64 deviceId{0};

  bool known() const noexcept { return !backend.isEmpty(); }
};

struct RenderResult
{
  bool ran{false};
  QString error;
  DeviceIdentity gpu; // what QRhi said it got, on every backend
  QImage frame;
  std::vector<QImage> extraFrames; // when extra grabs were requested
};

//! Parse the product's identity line:
//!   score.gfx: RHI device: backend=Vulkan device="NVIDIA GeForce RTX 4090"
//!   vendorId=0x10de deviceId=0x2684 deviceType=discrete
DeviceIdentity parseDeviceIdentity(const QString& line)
{
  static const QRegularExpression re{
      R"RX(score\.gfx: RHI device: backend=(\S+) device="(.*)" vendorId=0x([0-9a-f]+) )RX"
      R"RX(deviceId=0x([0-9a-f]+) deviceType=(\S+))RX"};
  DeviceIdentity id;
  const auto m = re.match(line);
  if(!m.hasMatch())
    return id;
  id.line = line.trimmed();
  id.backend = m.captured(1);
  id.deviceName = m.captured(2);
  id.vendorId = m.captured(3).toULongLong(nullptr, 16);
  id.deviceId = m.captured(4).toULongLong(nullptr, 16);
  id.deviceType = m.captured(5);
  return id;
}

//! The ONE remaining reason a leg of this file cannot be judged, and it is a
//! checked capability fact rather than a log-format accident: QRhi came up on
//! the Null backend, which validates and records commands and rasterises
//! nothing. Every assertion here reads pixels, so on Null they would all be
//! measuring a cleared frame. Everything else -- vendor, discrete vs
//! integrated, and llvmpipe/lavapipe (deviceType=cpu) -- renders correctly and
//! is asserted, not skipped.
//!
//! This is the same contract tests/gfx/GfxNullBackendRefuses.cpp pins for the
//! in-process fixture (P2-15, "the Null backend refuses rather than pretends"),
//! stated for the out-of-process harness: there the fixture knows the backend
//! because it selected it, here the app reports it. It is reachable, not
//! theoretical -- ScreenNode::createRenderState falls back to QRhi::Null
//! whenever no GPU backend can be created, with a qWarning that spells out this
//! exact hazard: "a harness that only checks 'the frame is not blank' passes on
//! a constant colour and reports success while verifying nothing".
void skipIfNothingIsRasterised(const RenderResult& r)
{
  if(r.gpu.backend == "Null")
    SKIP("QRhi::backendName() == \"Null\": this backend rasterises nothing, so "
         "no pixel assertion in this file can mean anything ("
         << r.gpu.line.toStdString() << ")");
}

//! Cross-leg guard for the multi-render cases. They compare frames produced by
//! separate app launches against each other, which is only an oracle if the
//! same device produced them; and it is only a CHECK if the identity is
//! actually known, which the old string compare of two empty scrapes was not.
void requireSameDevice(const RenderResult& r, const DeviceIdentity& ref)
{
  INFO("leg rendered by: " << r.gpu.line.toStdString()
                           << "\n  reference leg:  " << ref.line.toStdString());
  REQUIRE(r.gpu.known());
  REQUIRE(r.gpu.line == ref.line);
}

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
    if(line.contains("score.gfx: RHI device:"))
      if(const auto id = parseDeviceIdentity(line); id.known())
        r.gpu = id;

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
    double centre, int drawMode = 0, const QString& loaderUuid = {})
{
  return QStringLiteral(R"JS(
var UUID_WINDOW = "5a181207-7d40-4ad8-814e-879fcdf8cc31";
Score.createDevice("Window", UUID_WINDOW, {});
var s = Score.find("Scenario.1"); if (s) Score.remove(s);
var root = Score.rootInterval();
var loader = Score.createProcess(root, "%6", "%1");
if (!loader) { console.log("SCENE-ERROR: no loader"); Qt.exit(9); }
var md = Score.createProcess(root, "9ce44e4b-eeb6-4042-bb7f-9d0b28190daf", "");
if (!md) { console.log("SCENE-ERROR: no model display"); Qt.exit(9); }
Score.createCable(Score.outlet(loader, 0), Score.inlet(md, 1));
Score.setValue(Score.inlet(md, 2), [%4, %4, %4]);
Score.setValue(Score.inlet(md, 3), [%5, %5, %5]);
Score.setValue(Score.inlet(md, 4), %3);
Score.setValue(Score.inlet(md, 7), 6);
Score.setValue(Score.inlet(md, 8), %7);
Score.setValue(Score.inlet(md, 9), %2);
Score.setAddress(Score.outlet(md, 0), "Window:/");
Score.play();
)JS")
      .arg(assetPath)
      .arg(cameraMode)
      .arg(fovDeg)
      .arg(eye)
      .arg(centre)
      .arg(
          loaderUuid.isEmpty()
              ? QStringLiteral("5df71765-505f-4ab7-98c1-f305d10a01ef")
              : loaderUuid)
      .arg(drawMode);
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

//! More than one colour in the frame.
//!
//! nonBlank() is a MEAN, so a frame filled edge to edge with a single mid-grey
//! satisfies it. Measured, not supposed: a negative-control build of this file
//! that replaces every grab with a uniform RGB(128,128,128) passes the four
//! container cases and the container-family case outright, because their only
//! floor is nonBlank and their only oracle is "two legs agree" -- which two
//! identical flat fills satisfy perfectly.
//!
//! These cases render a lit cube against the black clear colour, so "something
//! was drawn" means at least two distinct pixel values, on every backend and
//! every vendor: there is no tolerance in it and nothing for a driver to
//! disagree about. The same negative control fails every one of them with this
//! floor in place.
bool nonUniform(const QImage& im)
{
  if(im.isNull() || im.width() * im.height() < 2)
    return false;
  const uchar* first = im.constScanLine(0);
  const int r0 = first[0], g0 = first[1], b0 = first[2];
  for(int y = 0; y < im.height(); y++)
  {
    const uchar* row = im.constScanLine(y);
    for(int x = 0; x < im.width(); x++)
      if(row[x * 3] != r0 || row[x * 3 + 1] != g0 || row[x * 3 + 2] != b0)
        return true;
  }
  return false;
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

//! What a single channel's field looks like, without pinning where it is.
//!
//! The three numbers are a shape, not a picture: the brightest value reached,
//! how many distinct non-zero levels it is quantised into, and how much of the
//! frame it covers. A term that has been switched off collapses all three to
//! zero; a term that has lost its per-fragment input collapses `levels` to 1
//! while leaving peak and area alone; a term that has flooded the frame moves
//! `area`. None of them moves when the FIELD MERELY SLIDES, which is exactly
//! the freedom the golden cannot be asked to pin (see the obj-cube case).
struct ChannelShape
{
  int peak{0};   //!< brightest value in the channel
  int levels{0}; //!< distinct non-zero values
  double area{0.0}; //!< fraction of the frame where the channel is non-zero
};

ChannelShape channelShape(const QImage& im, int channel)
{
  ChannelShape s;
  bool seen[256] = {};
  std::int64_t n = 0;
  for(int y = 0; y < im.height(); y++)
  {
    const uchar* row = im.constScanLine(y);
    for(int x = 0; x < im.width(); x++)
    {
      const int v = row[x * 3 + channel];
      if(v == 0)
        continue;
      n++;
      s.peak = std::max(s.peak, v);
      seen[v] = true;
    }
  }
  for(bool b : seen)
    s.levels += b ? 1 : 0;
  s.area = double(n) / (double(im.width()) * im.height());
  return s;
}

struct Diff
{
  double meanAbs{999};
  double fracFar{1}; // fraction of pixels off by > 24 codes in some channel
};

// Retained for the cross-LEG oracles below (fam-off vs fam-stl and friends),
// which compare two renders from the SAME run against each other rather than
// against a stored golden. Those are difference oracles -- "these two must
// agree" / "these two must differ" -- and are not subject to the golden
// tolerance, which is owned by compare.py via GoldenImage.hpp.
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

//! Where failure artifacts (golden / actual / diff) are written.
QString goldenArtifactDir()
{
#if defined(GOLDEN_ARTIFACT_DIR)
  return QStringLiteral(GOLDEN_ARTIFACT_DIR) + "/threedim-render";
#else
  return QDir::tempPath() + "/threedim-render-golden";
#endif
}

//! Probe the golden comparator BEFORE spending a render on it.
//!
//! requireMatchesGolden SKIPs when python3/numpy/PIL/scipy are missing, but it
//! could only find that out after the case had already driven a full app launch
//! and grab -- ~16 s of wall clock to reach a verdict of "no verdict".
//! goldenComparatorUsable() is a cached one-shot probe (GoldenImage.hpp), so
//! asking first costs nothing and skips in under a second.
void skipUnlessGoldenComparatorUsable()
{
  if(!score::testing::goldenComparatorUsable()
     || score::testing::goldenComparePath().isEmpty()
     || !QFileInfo::exists(score::testing::goldenComparePath()))
    SKIP("golden comparator unavailable (python3 + numpy/PIL/scipy); skipped "
         "before rendering rather than after");
}

//! Compare against the committed golden, or write it when
//! SCORE_THREEDIM_UPDATE_REFS=1 (used ONLY by a human who then LOOKS at it;
//! see the header — never bless an unjudged image).
//!
//! The golden is shared across backends. This used to SKIP unless the renderer
//! reported NVIDIA, which meant the comparison ran on exactly one vendor's
//! driver and was Skipped everywhere else -- on CI, on Mesa, on every
//! developer machine without that card. A golden that only one backend is ever
//! measured against cannot condemn a regression on any other, which is the
//! same defect the per-backend ref trees had, expressed as a skip instead of
//! as a directory.
//!
//! `channels` is forwarded to compare.py. It is not a tolerance: see
//! GoldenImage.hpp and compare.py's docstring for the one fact that licenses
//! narrowing it (a channel the renderer cannot reproduce against itself), and
//! the obj-cube case below for the only use of it in this file.
void requireMatchesGolden(
    const RenderResult& r, const QString& caseName,
    const QString& channels = QStringLiteral("rgb"))
{
  const QString refPath = refsDir() + "/" + caseName + ".png";
  if(qEnvironmentVariableIsSet("SCORE_THREEDIM_UPDATE_REFS"))
  {
    QDir().mkpath(refsDir());
    REQUIRE(nonBlank(r.frame));
    REQUIRE(r.frame.save(refPath));
    WARN("ref written (validate it visually before committing): "
         << refPath.toStdString());
    return;
  }

  if(!QFileInfo::exists(refPath))
    FAIL("no golden ref at " << refPath.toStdString()
                             << " (renders exist but were never validated?)");

  const auto v = score::testing::compareToGolden(
      r.frame, caseName, refsDir(), goldenArtifactDir(), channels);
  if(!v.ran)
    SKIP("golden comparator unavailable (python3 + numpy/PIL/scipy)");

  INFO(caseName.toStdString() << " on " << r.gpu.line.toStdString() << ": "
                              << v.metrics.toStdString());
  if(!v.pass)
    FAIL(caseName.toStdString()
         << ": " << v.reason.toStdString() << "\n  artifacts: "
         << v.artifacts.toStdString() << "/" << caseName.toStdString()
         << ".{golden,actual,diff}.png");
}
} // namespace

// THE GOLDEN HERE COVERS R AND G ONLY, AND THE REASON IS MEASURED.
//
// The Light projection (inlet 7 == 6) with an OBJ that carries UVs and normals
// selects ModelDisplayNode.cpp's phong pair, and that shader animates its own
// light off the transport clock:
//
//     lightPosition.y = sin(TIME) * 20.;
//     lightPosition.z = cos(TIME) * 50.;
//
// TIME is Node.cpp:41's `tk.date.impl / flicks_per_second`, so it is whatever
// the transport had reached at the frame the grab happened to catch. The
// materials route that animation into exactly one channel. Ambient is
// lightAmbient*materialAmbient == (0.01, 0.04, 0), constant. Diffuse is
// lightDiffuse*materialDiffuse == (0, 0.16, 0), GREEN-only, and the mesh is
// flat-shaded so it takes one value per face. Specular is
// lightSpecular*materialSpecular == (0, 0, 0.9), BLUE-only, scaled by
// pow(dotNH, 0.5) -- a square root, whose slope is unbounded as dotNH -> 0.
//
// So the frame is: R constant at 1, G a three-valued map of the FACE NORMALS
// (5 / 6 / 23 -- exactly what this case is named after), and B a smooth
// specular field with a terminator that a sub-percent light rotation drags
// across a pixel, swinging it by ~70 codes.
//
// Six independent renders on one machine (NVIDIA Quadro RTX 4000, OpenGL 4.6
// 595.84), all fifteen pairs:
//
//     max |dR| = 0      max |dG| = 0      pixels where R or G moved: 0
//     max |dB| up to 75, up to 5896 pixels (0.64 %) past the shared pixel_tol
//
// Two CONSECUTIVE runs scored max_abs 59 with 0.47 % of pixels over tolerance
// against each other -- so the blue channel fails compare.py's "self" profile,
// the bar that file demands of an image before it may become a reference at
// all. Re-blessing cannot help (the next run differs again) and widening the
// gate to 75 codes would let an inverted block through on every case in the
// tree. The channel simply has no golden.
//
// It is not dropped, it is asserted differently. What the animation moves is
// WHERE the specular field sits; what it leaves alone is the field's shape,
// and the shape is what a broken specular or a broken normal would destroy.
// Measured over the same six renders: peak 87 in all six, 87 distinct non-zero
// levels in all six, area 14.536 %..14.649 % of the frame. The floors below sit
// well under those, because the light's +100 x term anchors the highlight but
// its phase is not ours to pin.
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
  skipUnlessGoldenComparatorUsable();
  const auto r
      = renderScene(dir, "obj-cube", loaderScene(kGeometryLoader, obj, kProjLight));
  if(!r.error.isEmpty())
    SKIP(r.error.toStdString());
  skipIfNothingIsRasterised(r);

  // R + G: the silhouette and the per-face diffuse shading, i.e. the geometry
  // and the normals. Bit-exact against the golden on this machine (psnr=inf,
  // max_abs=0), and a one-code shift in G alone already fails the gate.
  requireMatchesGolden(r, "obj-cube", "rg");

  // B: the specular term the clock animates. Shape, not position.
  const auto spec = channelShape(r.frame, 2);
  INFO("specular (blue) field: peak " << spec.peak << ", " << spec.levels
                                      << " distinct levels, area "
                                      << 100.0 * spec.area << " % of frame");
  CHECK(spec.peak >= 48);    // measured 87 x6; 0 if the specular is gone
  CHECK(spec.levels >= 32);  // measured 87 x6; 1 if it lost its per-fragment
                             // half-vector and went flat
  CHECK(spec.area >= 0.05);  // measured 0.1454..0.1465
  CHECK(spec.area <= 0.35);  // and it must not flood the frame either
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
  skipIfNothingIsRasterised(r);
  // "the derived normals reach a lit pixel" is a statement about
  // GeometryLoader::deriveMissingNormals and the Light material, not about a
  // driver: nothing here is a vendor extension, an optional QRhi feature or a
  // precision-sensitive quantity. nonBlank() is meanLuma > 0.5/255 against a
  // measured 12.2, four orders of magnitude of headroom over the 2-code
  // cross-backend spread this campaign measured. Runs everywhere.
  INFO("rendered by: " << r.gpu.line.toStdString());
  CHECK(nonBlank(r.frame));
  CHECK(nonUniform(r.frame));
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
    skipIfNothingIsRasterised(r);
    // Same reasoning as the OBJ-without-normals case: the claim is that the
    // triplanar path has a lighting floor, which is shader source, not driver
    // behaviour. Runs on every backend and vendor.
    INFO("rendered by: " << r.gpu.line.toStdString());
    CHECK(nonBlank(r.frame));
    CHECK(nonUniform(r.frame));
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
  skipIfNothingIsRasterised(r);
  // As above: the triplanar lighting floor is shader source. Runs everywhere.
  INFO("rendered by: " << r.gpu.line.toStdString());
  CHECK(nonBlank(r.frame));
  CHECK(nonUniform(r.frame));
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
  skipIfNothingIsRasterised(ref);
  // The oracle below is OFF vs STL, and BOTH legs are rendered in this same
  // case, on this same machine, through this same backend -- a difference
  // oracle between two frames from one device, never against a stored image.
  // It therefore has no vendor content at all: whatever a driver does to a
  // flat-shaded cube it does identically to both legs, and only a change in
  // what the two LOADERS publish can move meanAbs off zero. That is why the
  // meanAbs<4 / fracFar<0.02 pair needs no backend tolerance and the case
  // needs no vendor gate.
  INFO("reference leg rendered by: " << ref.gpu.line.toStdString());
  REQUIRE(ref.gpu.known());
  REQUIRE(nonBlank(ref.frame));
  REQUIRE(nonUniform(ref.frame));

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
    // comparison, so it is checked rather than assumed. It now actually IS
    // checked -- the old form compared two log scrapes that were both empty on
    // every backend except OpenGL, so "" == "" passed unconditionally.
    requireSameDevice(r, ref.gpu);

    // Floor first, so a black frame names itself instead of surfacing as a
    // large diff of unclear origin -- and a FLAT frame too, since the oracle
    // below is an agreement one and two identical flat fills agree perfectly.
    CHECK(nonBlank(r.frame));
    CHECK(nonUniform(r.frame));
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
  skipIfNothingIsRasterised(r);
  // Per-container reachability floor; no driver content. Runs everywhere.
  INFO("rendered by: " << r.gpu.line.toStdString());
  CHECK(nonBlank(r.frame));
  CHECK(nonUniform(r.frame));
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
  DeviceIdentity refDevice;
  // What this case asserts about the four laws is a set of RATIOS between the
  // silhouette radii of four renders of one cube, all four produced on the same
  // device in this same case. A driver that scaled, biased or antialiased
  // differently would move all four radii together and cancel out of both the
  // ordering (a) and the one-parameter fit (b), whose free scale is refitted
  // from this run's own equidistant leg. The residual budget, 1% + 2 px, is
  // itself larger than the ~1 px an edge can move for the 2-code-out-of-255
  // cross-backend spread this campaign measured. Nothing vendor-specific is
  // touched: no extension, no optional QRhi feature, no fp64. Runs everywhere.
  for(const auto& L : laws)
  {
    const auto r = renderScene(
        dir, QStringLiteral("fisheye-%1").arg(L.name),
        fisheyeScene(obj, L.mode, kFisheyeFovDeg, kFisheyeEye, kCentre));
    if(!r.error.isEmpty())
      SKIP(r.error.toStdString());
    skipIfNothingIsRasterised(r);
    if(!refDevice.known())
    {
      REQUIRE(r.gpu.known());
      refDevice = r.gpu;
    }
    requireSameDevice(r, refDevice);

    const int drawn = drawnPixels(r.frame);
    const double rad = maxDrawnRadius(r.frame);
    INFO(L.name << ": mode " << L.mode << ", frame " << r.frame.width() << "x"
                << r.frame.height() << ", drawn " << drawn << " px, radius "
                << rad);
    REQUIRE(drawn > 64);
    REQUIRE(rad > 0.0);
    measured[L.mode] = rad;
  }

  INFO("rendered by: " << refDevice.line.toStdString());
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
    skipIfNothingIsRasterised(r);
    // The control leg of the pin: "a perspective render of a cube in front of
    // the camera draws more than 64 lit pixels". Measured in the thousands, and
    // no rasteriser draws a different NUMBER OF ORDERS OF MAGNITUDE. Gating it
    // on the vendor was worse than useless here: a [!shouldfail] case that
    // SKIPs is reported as skipped, not as failed, so on Vulkan, D3D11 and
    // D3D12 the pin was inert and would not have gone green when the axis bug
    // is fixed either.
    INFO("perspective control on " << r.gpu.line.toStdString() << ": "
                                   << drawnPixels(r.frame) << " px drawn");
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
// =============================================================================
// P2-7 -- a .vox model renders.
//
// THE RECORDED BLOCKER WAS WRONG, and the correction matters beyond this case.
// This case used to SKIP with "blocked on the Qt.vector3d zeroing defect
// (camera cannot be framed)". `Qt.vector3d(x,y,z)` really is dead in the
// console engine -- root cause below -- but it is not the only way to write a
// vec3 control, and the other way works:
//
//     Score.setValue(Score.inlet(md, 2), [-3.0, -3.0, -3.0]);
//
// That is EditContext.port.cpp:376-389, `setValue(QObject*, QList<qreal>)`,
// which takes a plain JS array. Every camera in this file's fisheye cases and
// the one below is written that way.
//
// Root cause of the Qt.vector3d defect, for whoever fixes it: the console
// engine (JS/ApplicationPlugin.hpp:52) IS a QQmlEngine, so `Qt` exists and
// `Qt.vector3d` resolves -- into libQt6Qml's QtObject::vector3d, which asks
// QQmlValueTypeProvider::createValueType for a QVector3D, gets no registered
// QML type, and returns the default-constructed out-variant WITHOUT checking
// the failure. The registration lives in QQuickVector3DValueType, inside
// libQt6Quick, and is only performed by qml_register_types_QtQuick() when
// something does `import QtQuick`. A headless `--no-gui --script` run never
// does. score_plugin_js.cpp:45-48 still has the Qt5-era hook for this, compiled
// out under `#if QT_VERSION < 6` and calling a function that no longer exists
// anywhere in the tree. Qt.vector2d/vector4d/quaternion/matrix4x4/rgba are dead
// the same way; Qt.rect/point/size work, because those value types live in
// libQt6Qml. Not fixed here -- the candidate fixes are a private generated
// symbol or a dlopen of the QtQuick plugin at startup, both of which want their
// own decision.
//
// WHAT THIS CASE CAN AND CANNOT ASSERT.
//
// The spec asks for "closed-form voxel colours from the palette", with
// "corrupt the palette buffer" as the negative control. That oracle has no
// consumer and the control cannot fire: VoxelLoader.cpp:143 publishes the
// 256-entry palette as an auxiliary buffer named "vox_palette", and grepping
// src/ for that name returns the producer and nothing else. No shader in the
// tree binds it, ModelDisplay included; in the real scores the consumer is user
// shader content. So a palette corruption is invisible to any in-repo render,
// and asserting a colour here would be asserting the material's lighting rather
// than the palette. Recorded rather than faked.
//
// What IS closed-form is the GEOMETRY, and it is the loader's own arithmetic.
// VoxelLoader's Mode combo defaults to init{1} = "Mesh (Simple)"
// (VoxelLoader.hpp:38-47), so the live path is VoxMeshFromFile, which places the
// model with an INTEGER pivot of `size / 2` (Vox.cpp:300). For the 2x2x2 solid
// makeTinyVox() writes, size/2 == 1 on every axis, so the surface mesh spans
// exactly [-1,1]^3 -- a fact of the loader, not of the renderer.
//
// The case renders it from three distances along the view axis at a fixed
// 60-degree perspective FOV and fits maxDrawnRadius() to
//     r(e) = tan(t_max(e)) / tan(fov/2)
// with t_max(e) computed from those eight corner coordinates. One free parameter
// (the px-per-NDC scale, fitted as the MEAN over the three legs, so no leg is
// privileged) against three measurements over a 3x range of distance -- and
// because t_max depends on the actual coordinates, the fit pins the loader's
// centring and unit scale, not merely the projection. Measured:
//
//   eye        t_max       r_ndc     measured px   scale px/NDC
//   -3,-3,-3   19.4712 deg 0.61237     219.73        358.82
//   -5,-5,-5   11.4218 deg 0.34993     125.50        358.65
//   -8,-8,-8    7.0108 deg 0.21300      75.98        356.72
//
// Residual against the mean scale is 0.21% / 0.16% / 0.38%, so the 1.5% + 2px
// gate is a real one.
//
// NEGATIVE CONTROL (run, see the ledger): neutralise the integer recentring
// pivot at Vox.cpp:300, which moves the mesh off the origin.
//
// Recorded while getting here: the Point Cloud mode (Mode 0) also renders, and
// its radii fit the same closed form to ~2% against voxel CENTRES at
// (+-0.5,+-0.5,+-0.5) -- the looser residual is the point sprite's fixed pixel
// size, which does not scale with distance and so does not cancel. The mesh leg
// is asserted because it is the default and therefore the path real documents
// take.
TEST_CASE(
    "a MagicaVoxel file renders through the voxel loader",
    "[integration][threedim][render][gui]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vox = dir.filePath("tiny.vox");
  {
    QFile f(vox);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(makeTinyVox());
  }

  constexpr double kVoxFovDeg = 60.0;
  const double h = (kVoxFovDeg * 0.5) * kFisheyePi / 180.0;

  // The eight corners of the surface mesh of a 2x2x2 solid after the loader's
  // integer recentring. This is the claim under test, so it is written out
  // rather than derived from anything the renderer says.
  const double kVoxCorners[8][3]{{-1, -1, -1}, {1, -1, -1}, {-1, 1, -1},
                                 {1, 1, -1},   {-1, -1, 1}, {1, -1, 1},
                                 {-1, 1, 1},   {1, 1, 1}};

  auto voxTmax = [&](double eye) {
    const double f = 1.0 / std::sqrt(3.0); // view axis: eye -> origin
    double t = 0.0;
    for(const auto& p : kVoxCorners)
    {
      const double vx = p[0] - eye, vy = p[1] - eye, vz = p[2] - eye;
      const double vl = std::sqrt(vx * vx + vy * vy + vz * vz);
      if(vl <= 0)
        continue;
      const double ct = std::clamp((vx * f + vy * f + vz * f) / vl, -1.0, 1.0);
      t = std::max(t, std::acos(ct));
    }
    return t;
  };

  const double eyes[3]{-3.0, -5.0, -8.0};
  double measured[3]{};
  int drawn[3]{};
  double rndc[3]{};
  // Same structure as the fisheye laws case, and the same reasoning: three
  // renders on ONE device, a monotone ordering with a 2 px margin, and a
  // one-parameter fit whose scale is the mean of this run's own three legs. Any
  // per-driver difference in how the silhouette is resolved is common to all
  // three legs and divides out. Runs on every backend and vendor.
  DeviceIdentity refDevice;

  for(int i = 0; i < 3; i++)
  {
    const auto r = renderScene(
        dir, QStringLiteral("vox-e%1").arg(int(-eyes[i])),
        // Camera mode 0 (Perspective) and draw mode 0 (Triangles): the surface
        // mesh the loader publishes by default, under the projection every real
        // Model Display uses. Tex. Proj. 6 = Light, so the faces are shaded
        // rather than relying on a texture nothing wires.
        fisheyeScene(
            vox, 0, kVoxFovDeg, eyes[i], 0.0, /*drawMode=*/0, kVoxelLoader));
    if(!r.error.isEmpty())
      SKIP(r.error.toStdString());
    skipIfNothingIsRasterised(r);
    if(!refDevice.known())
    {
      REQUIRE(r.gpu.known());
      refDevice = r.gpu;
    }
    requireSameDevice(r, refDevice);

    drawn[i] = drawnPixels(r.frame);
    measured[i] = maxDrawnRadius(r.frame);
    rndc[i] = std::tan(voxTmax(eyes[i])) / std::tan(h);

    INFO("eye " << eyes[i] << ": drawn " << drawn[i] << " px, radius "
                << measured[i] << " px, r_ndc " << rndc[i]);
    REQUIRE(drawn[i] > 1000);
    REQUIRE(measured[i] > 0.0);
  }

  INFO("rendered by: " << refDevice.line.toStdString());
  INFO("radii px: " << measured[0] << " " << measured[1] << " " << measured[2]
                    << "; drawn px: " << drawn[0] << " " << drawn[1] << " "
                    << drawn[2]);

  // Monotone: farther is smaller, in both extent and coverage. Cheap, and it
  // fails loudly if the camera control never reached the process at all.
  CHECK(measured[0] > measured[1] + 2.0);
  CHECK(measured[1] > measured[2] + 2.0);
  CHECK(drawn[0] > drawn[1]);
  CHECK(drawn[1] > drawn[2]);

  // The closed form. One free parameter over three measurements: the scale is
  // the mean, so two of the three legs are predictions.
  double scale = 0.0;
  for(int i = 0; i < 3; i++)
    scale += measured[i] / rndc[i];
  scale /= 3.0;

  for(int i = 0; i < 3; i++)
  {
    const double predicted = scale * rndc[i];
    INFO("eye " << eyes[i] << ": predicted " << predicted << " px, measured "
                << measured[i] << " px (scale " << scale << " px/NDC)");
    CHECK(std::abs(measured[i] - predicted) <= 0.015 * predicted + 2.0);
  }
}

TEST_CASE(
    "compute-shader-generated geometry rasterises",
    "[integration][threedim][render][gui]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  // syn-geo-asym-tri.cs: fixed VERTEX_COUNT, no TIME anywhere, so this case IS
  // goldenable (csf-vertex-count-expr.cs was tried first and is time-animated
  // by design: its particle cloud roams off-frame, five spaced grabs measured
  // all-blank on some runs).
  //
  // It used to render syn-geo-producer.cs, and that was the whole defect. That
  // shader exists to DRIVE other tests: it emits the standard oversized
  // fullscreen triangle, (-1,-1) (3,-1) (-1,3), every vertex the same flat
  // green. Rasterised, the frame it produces is 921600 pixels of exactly
  // (0,255,0) -- measured, one distinct colour, and the committed golden was
  // one distinct colour too. A comparison between two uniform fills is not a
  // comparison. Nothing about the geometry reached a pixel:
  //
  //   * every vertex is off-screen, so no edge and no corner is visible and
  //     any position error that still covers the viewport renders identically;
  //   * the colour is constant, so per-vertex colour interpolation is
  //     unobservable and a pipeline that ignored the `color` attribute
  //     entirely, or bound a constant, would pass;
  //   * only "the frame went black" could ever fail it, which is the one thing
  //     skipIfNothingIsRasterised and nonUniform already say.
  //
  // syn-geo-asym-tri.cs is the same pipeline with a triangle worth looking at:
  // fully on-screen, lopsided on both axes, red/green/blue corners. Its
  // silhouette is now 19.281 % of the frame in closed form, computed below from
  // the vertex coordinates in the shader rather than read off the golden, so a
  // wrong vertex position fails the AREA check even on a machine with no
  // golden and no comparator.
  skipUnlessGoldenComparatorUsable();
  const QString cs = gfxCorpusDir() + "/syn-geo-asym-tri.cs";
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
  skipIfNothingIsRasterised(r);

  // ---- what the compute shader wrote, read back off the screen ------------
  //
  // The golden pins the picture. These pin WHY it is that picture: they are
  // the closed form of syn-geo-asym-tri.cs's main(), so they hold on any
  // backend, on a machine with no python3, and against a golden nobody has
  // blessed yet. Between them and the golden there is nothing left for a
  // uniform frame to hide behind.

  // (1) The frame is not a flat fill. This is the assertion the old case could
  //     not make: its render and its golden were both one colour, so a Null
  //     backend's cleared frame and a correct render were indistinguishable.
  REQUIRE(nonUniform(r.frame));

  // (2) Coverage. A raw-raster pipeline writes gl_Position directly, so the
  //     [-1,1] clip square IS the viewport whatever its aspect, and the
  //     triangle's share of the frame is its share of that square:
  //         |(v1-v0) x (v2-v0)| / 2 / 4
  //       = |(1.35,0.40) x (0.70,1.35)| / 8
  //       = (1.8225 - 0.28) / 8 = 0.192813
  //     Measured here: 0.19281, five decimals of agreement -- Samples=1 is
  //     pinned in the harness config so there is no antialiased rim to
  //     account for, and the only cross-backend freedom left is which side of
  //     a fill-rule tie an edge pixel falls on. The triangle's perimeter is
  //     ~1900 px, 0.2 % of the frame, so a 1-px disagreement along the WHOLE
  //     boundary is 0.002; the band below is twice that.
  const double covered = double(drawnPixels(r.frame, 8))
                         / (double(r.frame.width()) * r.frame.height());
  INFO("triangle coverage " << 100.0 * covered
                            << " % of frame (closed form 19.2813 %)");
  CHECK(covered > 0.1888);
  CHECK(covered < 0.1968);

  // (3) The colour attribute is read PER VERTEX and interpolated. A constant
  //     colour -- the old shader's, or a pipeline that lost the attribute and
  //     fell back to one -- gives one value per channel; a Gouraud triangle
  //     between three primaries gives a wide gamut, and each channel spans
  //     nearly the full range on its own because it is 1 at its own corner and
  //     0 at the other two. Measured: 72997 distinct colours in the frame,
  //     254..255 distinct non-zero levels in every channel.
  for(int c = 0; c < 3; c++)
  {
    const auto s = channelShape(r.frame, c);
    INFO("channel " << c << ": peak " << s.peak << ", " << s.levels
                    << " distinct levels, area " << 100.0 * s.area << " %");
    CHECK(s.peak >= 200);   // measured 254..255: its own corner is saturated
    CHECK(s.levels >= 64);  // measured 254..255: it ramps away from that
                            // corner rather than switching
  }

  // (4) And the picture itself, which is where the triangle SITS -- the one
  //     thing the area and the ramps above cannot see, since both survive a
  //     flip, a rotation and an attribute permutation. Three runs of this
  //     scene were byte-identical, so unlike obj-cube's specular there is a
  //     real reference to compare against.
  requireMatchesGolden(r, "csf-geometry");
}




// =============================================================================
// P2-6 -- the attribute matrix, at the renderer.
//
// The CPU half is covered: GeometryLoaderFormats.cpp:223-237 pins the published
// attribute SET for an OBJ with UVs and normals (position / texcoord0 / normal /
// tangent, tangents derived by mikktspace whenever both are present --
// TinyObj.cpp:111 `gen_tangents = texcoords && normals`), and :279-280 pins OBJ
// vertex colours. What none of that says is whether any of it reaches a pixel.
//
// THREE ASSETS, one cube, differing only in which attributes the file carries:
//
//     full    v + vt + vn   -> pos / uv / normal / tangent
//     nonrm   v + vt        -> pos / uv / normal, the normals SYNTHESIZED by
//                              GeometryLoader::deriveMissingNormals; no tangent,
//                              because gen_tangents was decided in TinyObj
//                              before the derivation and the file had no `vn`
//     nouv    v +      vn   -> pos / normal. No uv, and no tangent either
//
// WHAT IS OBSERVABLE, measured rather than assumed. The obvious handle looked
// like ModelDisplay's `Tex. Proj.` combo (inlet 7), which names an attribute per
// entry. It does not work that way: measured, mode 0 ("Texture coordinates")
// with the full cube and no texture wired draws ZERO pixels. Every
// texture-projection mode emits the projected TEXTURE, and with nothing on the
// texture inlet that is black -- the same fact this file already documents for
// the VCG family. There is no UV-shading leg; what attribute presence changes is
// which MATERIAL the mesh lands on, and that is visible under Light.
//
// MEASURED, mode 6 (Light), mean luma over the frame:
//
//     full   4.81      nonrm  12.23      nouv  40.88
//     full  vs nonrm   meanAbs 15.01   fracFar 0.492
//     nonrm vs nouv    meanAbs 42.x    fracFar > 0.05
//
// TWO MEASURED FACTS, both recorded because both contradict a natural guess:
//
//  1. TANGENTS DO NOT REACH THE PICTURE at all through ModelDisplay. Forcing
//     `gen_tangents = false` in TinyObj -- which strips `full` of the only
//     attribute `nonrm` lacks besides authored normals -- moves `full`'s mean
//     luma from 4.8145 to 4.80818 and leaves the full-vs-nonrm difference at
//     meanAbs 15.00 / fracFar 0.491. So the tangent is transported
//     (ScenePreprocessorNode.cpp:2600 gives it vertex slot 3) and no in-tree
//     material reads it. In the real scores the consumer is user shader content.
//
//  2. Therefore the full-vs-nonrm difference is THE NORMALS, and a derived
//     normal is NOT the authored one on this path -- which is the opposite of
//     the VCG path, where the container-family case above measures OFF (no
//     normals in the file, derived) against STL (a normal per facet, authored)
//     agreeing inside meanAbs 4 on the same cube with the same winding. Open,
//     and reported rather than pinned: the two paths reach deriveMissingNormals
//     with the same eight corners and the same face list, so the difference is
//     upstream of it, in what TinyObj hands over for an OBJ that carries UVs.
//     Somebody should find out which; this case makes the disagreement visible
//     instead of leaving it between two files that never meet.
//
// WHAT IS ASSERTED:
//
//   * each of the three renders is NON-BLANK. `nonrm` in particular would be
//     black without deriveMissingNormals -- an unshaded mesh under the Light
//     projection has nothing to dot against. That is the "the loader either
//     supplies or derives" floor, and it is the assertion the negative control
//     reddens.
//   * each attribute difference MOVES THE FRAME: full != nonrm, nonrm != nouv.
//     The second compares two meshes that both lack tangents, which is what
//     isolates the UV.
//
// The remaining two of the four attributes:
//   NORMALS supplied vs derived is asserted as picture IDENTITY by the
//   container-family case above (OFF vs STL) -- the clean form, equal UV and
//   tangent status on both sides.
//   VERTEX COLOURS are asserted where the asset that has them lives:
//   ThreedimLitSceneTest.cpp's "glTF vertex colours reach the fragment stage"
//   renders BoxVertexColors.glb against a colour-blind control. OBJ vertex
//   colours are pinned on the CPU at GeometryLoaderFormats.cpp:279.
//
// NEGATIVE CONTROL (run, see the ledger): neuter deriveMissingNormals
// (GeometryLoader.cpp:239) so an OBJ with no `vn` keeps none.
// =============================================================================
TEST_CASE(
    "the attribute matrix: normals are derived when missing, and each "
    "attribute presence moves the frame",
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

  const QString full = write("attr-full.obj", makeCubeObj(true));
  const QString nouv = write("attr-nouv.obj", makeCubeObjNoUv());
  const QString nonrm = write("attr-nonrm.obj", makeCubeObj(false));

  // The three legs are compared against EACH OTHER, in one case, on one device.
  // What is asserted is that each attribute configuration reaches a pixel
  // (nonBlank, a mean-luma floor of 0.5/255 against measured 4.8/12.2/40.9) and
  // that the frames DIFFER (fracFar > 0.05, i.e. more than 5% of the frame off
  // by more than 24 codes -- measured 0.49). A driver difference of the 2 codes
  // this campaign measured across backends cannot manufacture or erase a
  // 24-code disagreement over half the frame. No vendor content; runs
  // everywhere.
  DeviceIdentity refDevice;
  auto render = [&](const char* name, const QString& asset) {
    const auto r = renderScene(
        dir, QString::fromUtf8(name),
        loaderScene(kGeometryLoader, asset, kProjLight));
    if(!r.error.isEmpty())
      SKIP(r.error.toStdString());
    skipIfNothingIsRasterised(r);
    if(!refDevice.known())
    {
      REQUIRE(r.gpu.known());
      refDevice = r.gpu;
    }
    requireSameDevice(r, refDevice);
    return r.frame;
  };

  const QImage liFull = render("attr-li-full", full);
  const QImage liNonrm = render("attr-li-nonrm", nonrm);
  const QImage liNouv = render("attr-li-nouv", nouv);

  INFO("rendered by: " << refDevice.line.toStdString());
  INFO(
      "mean luma: full=" << meanLuma(liFull) << " nonrm=" << meanLuma(liNonrm)
                         << " nouv=" << meanLuma(liNouv));

  // The floor: no attribute configuration is lost outright. `nonrm` carries no
  // normals in the file, so this is deriveMissingNormals or nothing.
  CHECK(nonBlank(liFull));
  CHECK(nonBlank(liNonrm));
  CHECK(nonBlank(liNouv));

  {
    const Diff d = diffImages(liFull, liNonrm);
    INFO("full vs nonrm (authored vs derived normals; tangents measured "
         "irrelevant): meanAbs="
         << d.meanAbs << " fracFar=" << d.fracFar);
    CHECK(d.fracFar > 0.05);
  }
  {
    const Diff d = diffImages(liNonrm, liNouv);
    INFO("nonrm vs nouv (UVs present vs absent, no tangents either side): "
         "meanAbs="
         << d.meanAbs << " fracFar=" << d.fracFar);
    CHECK(d.fracFar > 0.05);
  }
}
