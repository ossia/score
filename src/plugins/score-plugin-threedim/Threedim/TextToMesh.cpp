#include "TextToMesh.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <QFont>
#include <algorithm>
#include <cmath>

#include <QDebug>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QQuaternion>
#include <QRawFont>
#include <QString>
#include <QTransform>
#include <QVector>
#include <QtGui/private/qtriangulator_p.h>

#include <cmath>
#include <cstring>
#include <vector>

namespace Threedim
{

namespace
{
// Glyph outlines are triangulated in a scaled-up space: qTriangulate snaps
// vertices to a 1/32 grid and flattens curves in the coordinates it is given.
constexpr qreal kTriangulationUnitsPerPixelSize = 1024.;

// Triangulates one glyph outline with its non-zero fill rule (TrueType and CFF
// outlines are both defined with it), appending (x, -y, 0) positions and
// counter-clockwise triangles.
void tessellateGlyph(
    const QPainterPath& glyph, float px_size, float scale, float x_origin,
    float y_origin, std::vector<float>& out_positions,
    std::vector<uint32_t>& out_indices)
{
  QPainterPath path = glyph;
  path.setFillRule(Qt::WindingFill);

  const qreal k = kTriangulationUnitsPerPixelSize / qreal(px_size);
  const QTriangleSet set = qTriangulate(path, QTransform::fromScale(k, k), 1, true);
  if(set.indices.type() != QVertexIndexVector::UnsignedInt)
    return;

  const uint32_t base = uint32_t(out_positions.size() / 3);
  const int vertex_count = int(set.vertices.size() / 2);
  for(int i = 0; i < vertex_count; ++i)
  {
    out_positions.push_back(float(set.vertices[2 * i] / k) * scale + x_origin);
    out_positions.push_back(-float(set.vertices[2 * i + 1] / k) * scale + y_origin);
    out_positions.push_back(0.f);
  }

  const auto* idx = static_cast<const quint32*>(set.indices.data());
  const int index_count = set.indices.size() - set.indices.size() % 3;
  for(int t = 0; t < index_count; t += 3)
  {
    uint32_t a = idx[t], b = idx[t + 1], c = idx[t + 2];
    if(a == b || b == c || a == c)
      continue;
    const float* pa = &out_positions[(base + a) * 3];
    const float* pb = &out_positions[(base + b) * 3];
    const float* pc = &out_positions[(base + c) * 3];
    const float area
        = (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pb[1] - pa[1]) * (pc[0] - pa[0]);
    if(area == 0.f)
      continue;
    if(area < 0.f)
      std::swap(b, c);
    out_indices.push_back(base + a);
    out_indices.push_back(base + b);
    out_indices.push_back(base + c);
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
    // isValid() alone is not enough to say the family resolved. "Sans" is a
    // fontconfig alias with no meaning on Windows or macOS, and QRawFont there
    // comes back isValid() while resolving to NO family: familyName() is empty
    // and every character maps to .notdef. That is not cosmetic -- a space then
    // has a real outline (a hollow box), so " " renders as a box instead of
    // nothing and the rest of the text renders .notdef boxes too.
    //
    // An empty familyName is the reliable signal; supportsCharacter() still
    // answers true in that state, so it cannot be used for this.
    if(!rf.isValid() || rf.familyName().isEmpty())
    {
      // Fallback: default system font at the requested size, keeping the
      // requested weight and slant.
      QFont def;
      def.setPixelSize(inputs.font_size.value);
      def.setBold(inputs.bold.value);
      def.setItalic(inputs.italic.value);
      rf = QRawFont::fromFont(def);
    }

    const QString str = QString::fromStdString(inputs.text.value);

    // Pixel → world scale: QRawFont::pixelSize() is the nominal pixel
    // size. Height control sets the target cap height; we approximate
    // cap height as pixelSize × 0.7 (typical for Latin fonts).
    const float cap_ratio = 0.7f;
    // QRawFont::pixelSize() is not guaranteed to report the size that was
    // asked for: on Windows it comes back as -1, which would make
    // pixel_to_world NEGATIVE. A negative scale mirrors every polygon,
    // reversing its winding, and the ear clipper then emits no triangles at
    // all. Fall back to the size actually requested.
    const float px_size = rf.pixelSize() > 0 ? float(rf.pixelSize())
                                             : float(inputs.font_size.value);
    const float pixel_to_world
        = inputs.height.value / (px_size * cap_ratio + 1e-6f);

    std::vector<float> positions;
    std::vector<uint32_t> indices;
    positions.reserve(str.size() * 32 * 3);
    indices.reserve(str.size() * 32);

    const float line_spacing_px = float(rf.ascent() + rf.descent() + rf.leading());
    int glyph_count = 0;
    const QStringList lines = str.split(QLatin1Char('\n'));
    for(int li = 0; li < lines.size(); ++li)
    {
      QString line;
      line.reserve(lines[li].size());
      for(const QChar c : lines[li])
        if(c.category() != QChar::Other_Control)
          line.append(c);

      const QVector<quint32> glyphs = rf.glyphIndexesForString(line);
      const QVector<QPointF> advances = rf.advancesForGlyphIndexes(glyphs);
      glyph_count += glyphs.size();

      const float y_origin = -float(li) * line_spacing_px * pixel_to_world;
      const std::size_t line_start = positions.size();
      float cursor_x_px = 0.f;
      for(int gi = 0; gi < glyphs.size(); ++gi)
      {
        QPainterPath gp = rf.pathForGlyph(glyphs[gi]);
        if(!gp.isEmpty())
          tessellateGlyph(
              gp, px_size, pixel_to_world, cursor_x_px * pixel_to_world, y_origin,
              positions, indices);
        if(gi < advances.size())
          cursor_x_px += float(advances[gi].x());
      }

      if(inputs.center_x.value)
      {
        const float half = cursor_x_px * pixel_to_world * 0.5f;
        for(std::size_t v = line_start; v < positions.size(); v += 3)
          positions[v] -= half;
      }
    }

    if(positions.empty() || indices.empty())
    {
      // Say so. An empty mesh published silently looks healthy -- the font
      // resolves, the glyphs have outlines -- and the only symptom is an empty
      // scene state.
      qWarning(
          "TextToMesh: produced no geometry -- text=%d glyphs=%d pixelSize=%d "
          "px_size=%f scale=%f positions=%d indices=%d",
          int(str.size()), glyph_count, int(rf.pixelSize()),
          double(px_size), double(pixel_to_world), int(positions.size()),
          int(indices.size()));
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
  mat->stable_id = m_material_stable_id;
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
  m_wrapped_state.reset();
}

} // namespace Threedim
