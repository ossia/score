// Text to Mesh fills glyphs exactly as their outlines define them (N88).
//
// The published triangles must cover the same region as the glyph outline
// under the non-zero rule TrueType / CFF outlines are defined with: no
// triangle across a counter (the wedge in an italic 'e'), nothing missing for
// overlapping contours (synthetic bold). '\n' starts a new line and draws
// nothing itself.
//
// Coverage is sampled on a grid over each string's outline and compared with
// point-in-triangle over the published mesh. Font-independent: the reference
// outline comes from the same QRawFont resolution as the node. Without a
// system font directory the static Qt has no fonts; QT_QPA_FONTDIR is pointed
// at one when unset, and the test SKIPs if none is usable.

#include <Threedim/TextToMesh.hpp>

#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QPainterPath>
#include <QRawFont>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
void ensureApp()
{
  if(qApp)
    return;
  if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  if(!qEnvironmentVariableIsSet("QT_QPA_FONTDIR"))
  {
    for(const char* dir :
        {"/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/TTF",
         "/usr/share/fonts/dejavu", "/usr/share/fonts/truetype"})
    {
      if(QDir(QString::fromLatin1(dir)).exists())
      {
        qputenv("QT_QPA_FONTDIR", dir);
        break;
      }
    }
  }
  static int argc = 1;
  static char arg0[] = "TextToMeshTriangulationFixF";
  static char* argv[] = {arg0, nullptr};
  static auto* app = new QGuiApplication(argc, argv);
  (void)app;
}

constexpr int kPixelSize = 72;
// Height such that the node's pixel -> world scale is 1.
constexpr float kHeight = kPixelSize * 0.7f;

QRawFont resolveFont(bool bold, bool italic)
{
  QFont qf(QStringLiteral("Sans"));
  qf.setPixelSize(kPixelSize);
  qf.setBold(bold);
  qf.setItalic(italic);
  QRawFont rf = QRawFont::fromFont(qf);
  if(!rf.isValid() || rf.familyName().isEmpty())
  {
    QFont def;
    def.setPixelSize(kPixelSize);
    def.setBold(bold);
    def.setItalic(italic);
    rf = QRawFont::fromFont(def);
  }
  return rf;
}

bool fontUsable()
{
  ensureApp();
  QRawFont rf = resolveFont(false, false);
  if(!rf.isValid() || rf.familyName().isEmpty())
    return false;
  const auto g = rf.glyphIndexesForString(QStringLiteral("e"));
  return !g.isEmpty() && !rf.pathForGlyph(g[0]).isEmpty();
}

struct Mesh
{
  std::vector<float> pos;
  std::vector<uint32_t> idx;
  bool empty{true};
};

Mesh build(const std::string& text, bool bold, bool italic)
{
  ensureApp();
  auto n = std::make_unique<Threedim::TextToMesh>();
  n->inputs.text.value = text;
  n->inputs.height.value = kHeight;
  n->inputs.bold.value = bold;
  n->inputs.italic.value = italic;
  n->rebuild();
  (*n)();
  Mesh m;
  const auto& st = n->outputs.scene_out.scene.state;
  if(!st || !st->roots || st->roots->empty())
    return m;
  const auto& root = (*st->roots)[0];
  auto* mc = ossia::get_if<ossia::mesh_component_ptr>(&(*root->children)[1]);
  REQUIRE(mc);
  const auto& p = (*mc)->primitives.at(0);
  auto* pb = ossia::get_if<ossia::buffer_data>(&p.vertex_buffers.at(0)->resource);
  auto* ib = ossia::get_if<ossia::buffer_data>(&p.index_buffer->resource);
  REQUIRE(pb);
  REQUIRE(ib);
  const auto* pf = static_cast<const float*>(pb->data.get());
  const auto* pi = static_cast<const uint32_t*>(ib->data.get());
  m.pos.assign(pf, pf + p.vertex_count * 3);
  m.idx.assign(pi, pi + p.index_count);
  m.empty = false;
  return m;
}

// The string's outline in Qt pixel space (Y down), laid out like the node.
QPainterPath referenceOutline(const QString& str, bool bold, bool italic)
{
  QRawFont rf = resolveFont(bold, italic);
  const auto glyphs = rf.glyphIndexesForString(str);
  const auto advances = rf.advancesForGlyphIndexes(glyphs);
  QPainterPath out;
  out.setFillRule(Qt::WindingFill);
  qreal x = 0;
  for(int i = 0; i < glyphs.size(); ++i)
  {
    QPainterPath gp = rf.pathForGlyph(glyphs[i]);
    gp.translate(x, 0);
    out.addPath(gp);
    x += advances[i].x();
  }
  out.setFillRule(Qt::WindingFill);
  return out;
}

bool insideMesh(const Mesh& m, float x, float y)
{
  for(std::size_t t = 0; t + 2 < m.idx.size(); t += 3)
  {
    const float* a = &m.pos[m.idx[t] * 3];
    const float* b = &m.pos[m.idx[t + 1] * 3];
    const float* c = &m.pos[m.idx[t + 2] * 3];
    const float d1 = (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
    const float d2 = (c[0] - b[0]) * (y - b[1]) - (c[1] - b[1]) * (x - b[0]);
    const float d3 = (a[0] - c[0]) * (y - c[1]) - (a[1] - c[1]) * (x - c[0]);
    if(d1 >= 0.f && d2 >= 0.f && d3 >= 0.f)
      return true;
  }
  return false;
}

struct Coverage
{
  int inside{};
  int missing{};
  int spurious{};
};

Coverage compare(const std::string& text, bool bold, bool italic)
{
  const Mesh m = build(text, bold, italic);
  REQUIRE_FALSE(m.empty);
  const QPainterPath ref
      = referenceOutline(QString::fromStdString(text), bold, italic);
  const QRectF r = ref.boundingRect().adjusted(-2, -2, 2, 2);
  const float scale = kHeight / (kPixelSize * 0.7f + 1e-6f);

  Coverage c;
  constexpr qreal step = 0.37;
  for(qreal y = r.top(); y < r.bottom(); y += step)
    for(qreal x = r.left(); x < r.right(); x += step)
    {
      const bool in_ref = ref.contains(QPointF{x, y});
      const bool in_mesh = insideMesh(m, float(x) * scale, -float(y) * scale);
      c.inside += in_ref;
      c.missing += in_ref && !in_mesh;
      c.spurious += !in_ref && in_mesh;
    }
  return c;
}

void checkCoverage(const std::string& text, bool bold, bool italic)
{
  CAPTURE(text, bold, italic);
  const Coverage c = compare(text, bold, italic);
  CAPTURE(c.inside, c.missing, c.spurious);
  REQUIRE(c.inside > 0);
  CHECK(c.missing <= c.inside / 200);
  CHECK(c.spurious <= c.inside / 200);
}

void bounds(const Mesh& m, float lo[2], float hi[2])
{
  lo[0] = lo[1] = 1e30f;
  hi[0] = hi[1] = -1e30f;
  for(const auto i : m.idx)
    for(int k = 0; k < 2; k++)
    {
      lo[k] = std::min(lo[k], m.pos[i * 3 + k]);
      hi[k] = std::max(hi[k], m.pos[i * 3 + k]);
    }
}
}

TEST_CASE("Text to Mesh covers exactly the glyph outlines", "[threedim][text_to_mesh][fixF]")
{
  if(!fontUsable())
    SKIP("no usable scalable font on this host");

  checkCoverage("o", false, false);
  checkCoverage("e", false, true);
  checkCoverage("Hello", false, true);
  checkCoverage("B", true, false);
  checkCoverage("ossia", true, false);
  checkCoverage("ossia", true, true);
}

TEST_CASE("Text to Mesh breaks lines on newline", "[threedim][text_to_mesh][fixF]")
{
  if(!fontUsable())
    SKIP("no usable scalable font on this host");

  CHECK(build("\n", false, false).empty);

  const Mesh one = build("H", false, false);
  const Mesh two = build("H\nH", false, false);
  REQUIRE_FALSE(one.empty);
  REQUIRE_FALSE(two.empty);
  CHECK(two.idx.size() == 2 * one.idx.size());

  float lo1[2], hi1[2], lo2[2], hi2[2];
  bounds(one, lo1, hi1);
  bounds(two, lo2, hi2);
  CHECK(lo2[0] == Approx(lo1[0]).margin(1e-3));
  CHECK(hi2[0] == Approx(hi1[0]).margin(1e-3));
  CHECK(hi2[1] == Approx(hi1[1]).margin(1e-3));

  const QRawFont rf = resolveFont(false, false);
  const float spacing = float(rf.ascent() + rf.descent() + rf.leading())
                        * (kHeight / (kPixelSize * 0.7f + 1e-6f));
  CHECK(lo2[1] == Approx(lo1[1] - spacing).margin(1e-2));
}
