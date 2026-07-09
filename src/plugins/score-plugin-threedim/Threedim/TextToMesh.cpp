#include "TextToMesh.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <QFont>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QQuaternion>
#include <QRawFont>
#include <QString>
#include <QTransform>
#include <QVector>

#include <cmath>
#include <cstring>
#include <vector>

namespace Threedim
{

namespace
{

// ─── Ear-clipping triangulator ────────────────────────────────────────
//
// Handles simple (non-self-intersecting, no holes) polygons in CCW
// winding order. For each emitted triangle, the resulting indices
// reference positions in the input polygon's order.
//
// Complexity: O(n²). Glyphs flatten to dozens of verts at most, so
// acceptable. For large pts-per-glyph or paragraph text later, swap
// for earcut.hpp.

struct Vec2 { float x, y; };

inline float triSign(Vec2 a, Vec2 b, Vec2 c) noexcept
{
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

inline bool pointInTri(Vec2 p, Vec2 a, Vec2 b, Vec2 c) noexcept
{
  const float d1 = triSign(p, a, b);
  const float d2 = triSign(p, b, c);
  const float d3 = triSign(p, c, a);
  const bool neg = (d1 < 0.f) || (d2 < 0.f) || (d3 < 0.f);
  const bool pos = (d1 > 0.f) || (d2 > 0.f) || (d3 > 0.f);
  return !(neg && pos);
}

// Signed area × 2. Positive = CCW in a Y-up frame.
float polyArea(const std::vector<Vec2>& p) noexcept
{
  float s = 0.f;
  const std::size_t n = p.size();
  for(std::size_t i = 0; i < n; ++i)
  {
    const auto& a = p[i];
    const auto& b = p[(i + 1) % n];
    s += a.x * b.y - b.x * a.y;
  }
  return s;
}

// Ear-clip `poly` into triangles; append indices (into `base_offset +
// original polygon index`) to `out_indices`.
void earClip(
    const std::vector<Vec2>& poly, uint32_t base_offset,
    std::vector<uint32_t>& out_indices)
{
  const std::size_t n0 = poly.size();
  if(n0 < 3)
    return;

  // Make a working copy of the polygon with flipped winding if needed
  // so the triangulator always sees CCW.
  std::vector<int> idx(n0);
  if(polyArea(poly) < 0.f)
  {
    for(std::size_t i = 0; i < n0; ++i)
      idx[i] = int(n0 - 1 - i);
  }
  else
  {
    for(std::size_t i = 0; i < n0; ++i)
      idx[i] = int(i);
  }

  int n = (int)idx.size();
  int guard = n * 3; // bail to avoid infinite loop on degenerate input
  while(n > 3 && guard-- > 0)
  {
    bool ear_found = false;
    for(int i = 0; i < n; ++i)
    {
      const int ip = (i + n - 1) % n;
      const int in_ = (i + 1) % n;
      const Vec2 a = poly[idx[ip]];
      const Vec2 b = poly[idx[i]];
      const Vec2 c = poly[idx[in_]];
      if(triSign(a, b, c) <= 0.f)
        continue; // reflex or collinear — not an ear
      bool blocked = false;
      for(int j = 0; j < n; ++j)
      {
        if(j == ip || j == i || j == in_)
          continue;
        if(pointInTri(poly[idx[j]], a, b, c))
        {
          blocked = true;
          break;
        }
      }
      if(blocked)
        continue;
      out_indices.push_back(base_offset + uint32_t(idx[ip]));
      out_indices.push_back(base_offset + uint32_t(idx[i]));
      out_indices.push_back(base_offset + uint32_t(idx[in_]));
      idx.erase(idx.begin() + i);
      --n;
      ear_found = true;
      break;
    }
    if(!ear_found)
      break; // give up on degenerate polygons
  }
  if(n == 3)
  {
    out_indices.push_back(base_offset + uint32_t(idx[0]));
    out_indices.push_back(base_offset + uint32_t(idx[1]));
    out_indices.push_back(base_offset + uint32_t(idx[2]));
  }
}

// Convert a QPainterPath's filled polygons into (positions, indices),
// appending to out_positions / out_indices. Positions are emitted as
// (x, y_flipped, 0). `scale` maps Qt pixel coords to world units.
void tessellatePath(
    const QPainterPath& path, float scale, float x_origin,
    std::vector<float>& out_positions, std::vector<uint32_t>& out_indices)
{
  // toFillPolygons flattens curves and returns one or more polygons
  // representing the filled region. Holes would appear as separate
  // polygons with opposite winding — in this v1 we treat every polygon
  // as a solid fill.
  const QList<QPolygonF> polys = path.toFillPolygons();
  for(const auto& qpoly : polys)
  {
    if(qpoly.size() < 3)
      continue;
    std::vector<Vec2> poly;
    poly.reserve(qpoly.size());
    // Skip the closing duplicate vertex that Qt tends to append.
    int count = qpoly.size();
    if(count > 1 && qpoly[0] == qpoly[count - 1])
      count--;
    for(int i = 0; i < count; ++i)
    {
      const auto& p = qpoly[i];
      // Y flip so the mesh uses a right-handed Y-up frame (Qt is Y-down).
      poly.push_back({float(p.x() * scale + x_origin),
                      float(-p.y() * scale)});
    }
    const uint32_t base = uint32_t(out_positions.size() / 3);
    for(const auto& v : poly)
    {
      out_positions.push_back(v.x);
      out_positions.push_back(v.y);
      out_positions.push_back(0.f);
    }
    earClip(poly, base, out_indices);
  }
}

} // namespace

void TextToMesh::rebuild()
{
  const bool text_inputs_changed
      = m_cached_text != inputs.text.value
        || m_cached_family != inputs.font_family.value
        || m_cached_size != inputs.font_size.value
        || m_cached_bold != inputs.bold.value
        || m_cached_italic != inputs.italic.value
        || m_cached_height != inputs.height.value
        || m_cached_center != inputs.center_x.value;

  float scratch[16];
  CachedTRS xformCache = m_cachedTRS;
  computeTRSMatrix(inputs, scratch, xformCache);
  m_cachedTRS = xformCache;

  // Rebuild the mesh only when the text / font parameters changed.
  // Pure TRS edits keep the same mesh_component and just bump the
  // enclosing scene_state version.
  if(text_inputs_changed || !m_cached_mesh)
  {
    m_cached_text = inputs.text.value;
    m_cached_family = inputs.font_family.value;
    m_cached_size = inputs.font_size.value;
    m_cached_bold = inputs.bold.value;
    m_cached_italic = inputs.italic.value;
    m_cached_height = inputs.height.value;
    m_cached_center = inputs.center_x.value;

    // Build a QRawFont from the requested family. QRawFont::fromFont
    // resolves aliases (e.g. "Sans" → the system default).
    QFont qf(QString::fromStdString(inputs.font_family.value));
    qf.setPixelSize(inputs.font_size.value);
    qf.setBold(inputs.bold.value);
    qf.setItalic(inputs.italic.value);
    QRawFont rf = QRawFont::fromFont(qf);
    if(!rf.isValid())
    {
      // Fallback: default system font at the requested size.
      QFont def;
      def.setPixelSize(inputs.font_size.value);
      rf = QRawFont::fromFont(def);
    }

    const QString str = QString::fromStdString(inputs.text.value);
    const QVector<quint32> glyphs = rf.glyphIndexesForString(str);
    const QVector<QPointF> advances = rf.advancesForGlyphIndexes(glyphs);

    // Pixel → world scale: QRawFont::pixelSize() is the nominal pixel
    // size. Height control sets the target cap height; we approximate
    // cap height as pixelSize × 0.7 (typical for Latin fonts).
    const float cap_ratio = 0.7f;
    const float pixel_to_world
        = inputs.height.value
          / (float(rf.pixelSize()) * cap_ratio + 1e-6f);

    std::vector<float> positions;
    std::vector<uint32_t> indices;
    positions.reserve(glyphs.size() * 32 * 3);
    indices.reserve(glyphs.size() * 32);

    float cursor_x_px = 0.f;
    for(int gi = 0; gi < glyphs.size(); ++gi)
    {
      QPainterPath gp = rf.pathForGlyph(glyphs[gi]);
      if(!gp.isEmpty())
        tessellatePath(
            gp, pixel_to_world, cursor_x_px * pixel_to_world,
            positions, indices);
      if(gi < advances.size())
        cursor_x_px += float(advances[gi].x());
    }

    // Optionally center the text on X — total advance is where we
    // ended up at cursor_x_px; shift all vertices by -half.
    if(inputs.center_x.value && !positions.empty())
    {
      const float half = cursor_x_px * pixel_to_world * 0.5f;
      for(std::size_t v = 0; v < positions.size(); v += 3)
        positions[v] -= half;
    }

    if(positions.empty() || indices.empty())
    {
      // Empty string or unrenderable font — keep m_wrapped_state valid
      // (reset mesh) but clear its content so republish emits empty.
      m_cached_mesh.reset();
      if(!m_wrapped_state)
        m_wrapped_state = std::make_shared<ossia::scene_state>();
      m_wrapped_state->roots.reset();
      m_wrapped_state->materials.reset();
      m_wrapped_state->version = ++m_version_counter;
      m_wrapped_state->dirty_index = m_version_counter;
      m_pending_dirty = 0xFF;
      return;
    }

    // Build position / normal / texcoord buffers.
    const std::size_t vcount = positions.size() / 3;
    auto pos_buf = std::make_shared<std::vector<float>>(std::move(positions));
    auto nrm_buf = std::make_shared<std::vector<float>>(vcount * 3, 0.f);
    for(std::size_t i = 0; i < vcount; ++i)
      (*nrm_buf)[i * 3 + 2] = 1.f; // +Z normal
    auto uv_buf = std::make_shared<std::vector<float>>(vcount * 2, 0.f);
    auto idx_buf = std::make_shared<std::vector<uint32_t>>(std::move(indices));

    auto make_res = [](std::shared_ptr<std::vector<float>> b,
                       ossia::buffer_data::usage u) {
      auto r = std::make_shared<ossia::buffer_resource>();
      ossia::buffer_data bd;
      bd.data = std::shared_ptr<const void>(b, b->data());
      bd.byte_size = int64_t(b->size() * sizeof(float));
      bd.usage_hint = u;
      r->resource = std::move(bd);
      r->dirty_index = 1;
      return r;
    };

    ossia::mesh_primitive mp;
    // Stable id: unique per rebuilt mesh, stable across TRS-only edits
    // (this block only runs when text/font inputs changed). Must be
    // nonzero for the registry's mesh-slab allocator, and must never
    // repeat ACROSS PRODUCERS — the registry keys slabs on the bare id
    // process-wide, so it has to come from the global mint (like
    // PBRMesh/Light/the loaders do), not a per-instance counter.
    mp.stable_id = ossia::mint_stable_id();
    mp.topology = ossia::primitive_topology::triangles;
    mp.vertex_count = uint32_t(vcount);
    mp.index_count = uint32_t(idx_buf->size());
    // Local-space AABB over the tessellated glyph positions. Enables GPU
    // frustum / occlusion culling in downstream scene filters.
    mp.bounds = ossia::compute_aabb_from_positions(pos_buf->data(), vcount);
    // No material_component — consumer applies default factors.

    uint32_t bi = 0;
    auto push_attr = [&](std::shared_ptr<std::vector<float>> b,
                         int floats_per_vertex,
                         ossia::attribute_semantic sem,
                         ossia::vertex_format fmt) {
      mp.vertex_buffers.push_back(
          make_res(b, ossia::buffer_data::usage::vertex_buffer));
      ossia::vertex_attribute a;
      a.semantic = sem;
      a.format = fmt;
      a.buffer_index = bi++;
      a.byte_offset = 0;
      a.byte_stride = uint32_t(floats_per_vertex) * sizeof(float);
      a.rate = ossia::vertex_attribute::input_rate::per_vertex;
      mp.attributes.push_back(a);
    };
    push_attr(pos_buf, 3, ossia::attribute_semantic::position, ossia::vertex_format::float3);
    push_attr(nrm_buf, 3, ossia::attribute_semantic::normal, ossia::vertex_format::float3);
    push_attr(uv_buf, 2, ossia::attribute_semantic::texcoord0, ossia::vertex_format::float2);

    {
      auto ib = std::make_shared<ossia::buffer_resource>();
      ossia::buffer_data bd;
      bd.data = std::shared_ptr<const void>(idx_buf, idx_buf->data());
      bd.byte_size = int64_t(idx_buf->size() * sizeof(uint32_t));
      bd.usage_hint = ossia::buffer_data::usage::index_buffer;
      ib->resource = std::move(bd);
      ib->dirty_index = 1;
      mp.index_buffer = std::move(ib);
      mp.index_type = ossia::index_format::uint32;
    }

    auto mc = std::make_shared<ossia::mesh_component>();
    mc->primitives.push_back(std::move(mp));
    mc->dirty_index = 1;
    m_cached_mesh = std::move(mc);
  }

  // Build scene_node tree: root { scene_transform, mesh_component }.
  ossia::scene_transform xform;
  xform.translation[0] = inputs.position.value.x;
  xform.translation[1] = inputs.position.value.y;
  xform.translation[2] = inputs.position.value.z;
  auto q = QQuaternion::fromEulerAngles(
      inputs.rotation.value.x, inputs.rotation.value.y,
      inputs.rotation.value.z);
  xform.rotation[0] = q.x();
  xform.rotation[1] = q.y();
  xform.rotation[2] = q.z();
  xform.rotation[3] = q.scalar();
  xform.scale[0] = inputs.scale.value.x;
  xform.scale[1] = inputs.scale.value.y;
  xform.scale[2] = inputs.scale.value.z;
  xform.raw_slot = m_xform_ref;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(xform);
  children->push_back(ossia::mesh_component_ptr(m_cached_mesh));

  auto node = std::make_shared<ossia::scene_node>();
  node->name = "Text";
  node->children = std::move(children);
  node->dirty_index = ++m_version_counter;

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(node));

  // One default material so downstream PBR has something to bind.
  auto mat = std::make_shared<ossia::material_component>();
  mat->base_color_factor[0] = 1.f;
  mat->base_color_factor[1] = 1.f;
  mat->base_color_factor[2] = 1.f;
  mat->base_color_factor[3] = 1.f;
  auto mats = std::make_shared<std::vector<ossia::material_component_ptr>>();
  mats->push_back(std::move(mat));

  if(!m_wrapped_state)
    m_wrapped_state = std::make_shared<ossia::scene_state>();
  m_wrapped_state->roots = std::move(roots);
  m_wrapped_state->materials = std::move(mats);
  m_wrapped_state->version = m_version_counter;
  m_wrapped_state->dirty_index = m_version_counter;
  m_pending_dirty = 0xFF;
}

void TextToMesh::operator()()
{
  if(!m_wrapped_state)
    rebuild();
  outputs.scene_out.scene.state = m_wrapped_state;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

void TextToMesh::init(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  if(!raw_transform_slot.valid())
  {
    raw_transform_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawTransform,
        sizeof(score::gfx::RawLocalTransform));
    m_xform_ref = r.registry().toOssiaRef(raw_transform_slot);
  }
  if(raw_transform_slot.valid())
  {
    score::gfx::RawLocalTransform seed{};
    r.registry().updateSlot(res, raw_transform_slot, &seed, sizeof(seed));
  }
}

void TextToMesh::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(!raw_transform_slot.valid())
    return;

  score::gfx::RawLocalTransform xform{};
  xform.translation[0] = inputs.position.value.x;
  xform.translation[1] = inputs.position.value.y;
  xform.translation[2] = inputs.position.value.z;
  QQuaternion q = QQuaternion::fromEulerAngles(
      inputs.rotation.value.x, inputs.rotation.value.y,
      inputs.rotation.value.z);
  xform.rotation[0] = q.x();
  xform.rotation[1] = q.y();
  xform.rotation[2] = q.z();
  xform.rotation[3] = q.scalar();
  xform.scale[0] = inputs.scale.value.x;
  xform.scale[1] = inputs.scale.value.y;
  xform.scale[2] = inputs.scale.value.z;
  r.registry().updateSlot(res, raw_transform_slot, &xform, sizeof(xform));
}

void TextToMesh::release(score::gfx::RenderList& r)
{
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_xform_ref = {};
  // Producer-state-drift Option A — see Light::release.
  m_wrapped_state.reset();
}

} // namespace Threedim
