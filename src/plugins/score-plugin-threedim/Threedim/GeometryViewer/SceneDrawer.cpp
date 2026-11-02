#include "SceneDrawer.hpp"

#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

#include <Threedim/GeometryViewer/Node.hpp>
#include <Threedim/GeometryViewer/Process.hpp>
#include <Threedim/ShaderLib.hpp>

#include <QImage>

#include <cmath>
#include <cstring>
#include <numbers>

namespace Gfx::GeometryViewer
{
namespace
{
//! The attributes a shader variant reads, and whether it draws points
enum VariantBits : uint8_t
{
  variant_normal = 1,
  variant_uv = 2,
  variant_color = 4,
  variant_points = 8
};

// The uniforms every shader of the Model Display renderer declares: renderer
// at 0, process at 1, camera at 2. The camera's view and projection are those
// of the viewer; the model matrix of each draw comes from its own block.
#define scene_default_uniforms \
  "\
layout(std140, binding = 0) uniform renderer_t { \n\
  mat4 clipSpaceCorrMatrix; \n\
  vec2 renderSize; \n\
} renderer; \n\
layout(std140, binding = 1) uniform process_t { \n\
  float TIME; \n\
  float TIMEDELTA; \n\
  float PROGRESS; \n\
  float SAMPLERATE; \n\
  int PASSINDEX; \n\
  int FRAMEINDEX; \n\
  vec2 RENDERSIZE; \n\
  vec4 DATE; \n\
} isf_process_uniforms; \n\
layout(std140, binding = 2) uniform camera_t { \n\
  mat4 matrixModelViewProjection; \n\
  mat4 matrixModelView; \n\
  mat4 matrixModel; \n\
  mat4 matrixView; \n\
  mat4 matrixProjection; \n\
  mat3 matrixNormal; \n\
  float fov; \n\
  float near; \n\
  float far; \n\
} camera; \n\
"

// Material is MaterialGPU, with the field names of the shaderlib's TYPES
// block (score_scene_common.glsl); model and normal are its PerDraw's.
#define scene_draw_uniforms \
  "\
struct Material { \n\
  vec4 baseColor; \n\
  vec4 metallicRoughnessOcclusionUnlit; \n\
  vec4 emissive_strength; \n\
  uvec4 textureRefs; \n\
  uint feature_mask; \n\
  uint hit_group_id; \n\
  uint occlusion_textureRef; \n\
  float alpha_cutoff; \n\
}; \n\
layout(std140, binding = 4) uniform draw_t { \n\
  mat4 model; \n\
  mat4 normal; \n\
  Material material; \n\
  vec4 uv_offset_scale; \n\
  vec4 params; \n\
} draw; \n\
layout(std140, binding = 5) uniform lighting_t { \n\
  vec4 eye; \n\
  vec4 key; \n\
  vec4 fill; \n\
  vec4 wire; \n\
} lighting; \n\
layout(binding = 3) uniform sampler2D base_color_tex; \n\
layout(binding = 6) uniform sampler2D emissive_tex; \n\
"

const constexpr auto scene_vertex_shader = R"_(
layout(location = 0) in vec3 position;
#if defined(SCENE_NORMAL)
layout(location = 1) in vec3 normal;
#endif
#if defined(SCENE_UV)
layout(location = 2) in vec2 texcoord;
#endif
#if defined(SCENE_COLOR)
layout(location = 3) in vec4 color;
#endif

layout(location = 0) out vec3 v_world;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_uv;
layout(location = 3) out vec4 v_color;
layout(location = 4) out vec2 v_bary;

)_" scene_default_uniforms scene_draw_uniforms R"_(

#if defined(SCENE_POINTS)
out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };
#else
out gl_PerVertex { vec4 gl_Position; };
#endif

void main()
{
  vec4 world = draw.model * vec4(position, 1.0);
  v_world = world.xyz;
#if defined(SCENE_NORMAL)
  v_normal = mat3(draw.normal) * normal;
#else
  v_normal = vec3(0.0);
#endif
#if defined(SCENE_UV)
  v_uv = texcoord;
#else
  v_uv = vec2(0.0);
#endif
#if defined(SCENE_COLOR)
  v_color = color;
#else
  v_color = vec4(1.0);
#endif
  // The corners of each triangle of a triangle list, for the wireframe where
  // lines cannot be rasterised
  const int corner = gl_VertexIndex % 3;
  v_bary = corner == 0 ? vec2(0.0, 0.0) : corner == 1 ? vec2(0.0, 1.0) : vec2(1.0, 0.0);

  gl_Position = renderer.clipSpaceCorrMatrix * camera.matrixProjection * camera.matrixView * world;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = -gl_Position.y;
#endif
#if defined(SCENE_POINTS)
  gl_PointSize = draw.params.z;
#endif
}
)_";

const constexpr auto scene_fragment_shader = R"_(
layout(location = 0) in vec3 v_world;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in vec4 v_color;
layout(location = 4) in vec2 v_bary;
layout(location = 0) out vec4 fragColor;

)_" scene_draw_uniforms R"_(

// The viewer's modes, Gfx::GeometryViewer::Mode
const int MODE_SHADED = 0;
const int MODE_WIREFRAME = 1;
const int MODE_POINTS = 2;
const int MODE_VERTEX_COLORS = 3;
const int MODE_TEXCOORDS = 4;

// Lighting is computed in linear light, then encoded for the 8-bit target
vec3 encode(vec3 c)
{
  return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
}

vec2 materialUV(vec2 uv)
{
  // KHR_texture_transform: scale, then rotate, then offset
  const float r = draw.params.w;
  const vec2 s = uv * draw.uv_offset_scale.zw;
  const float c = cos(r), n = sin(r);
  return vec2(c * s.x + n * s.y, -n * s.x + c * s.y) + draw.uv_offset_scale.xy;
}

void main()
{
  const int mode = int(draw.params.x + 0.5);
  const Material m = draw.material;

  // Which textures are bound: 1 base colour, 2 emissive
  const int textures = int(draw.params.y + 0.5);
  vec4 base = m.baseColor;
#if defined(SCENE_UV)
  if((textures & 1) != 0)
    base *= texture(base_color_tex, materialUV(v_uv));
#endif
#if defined(SCENE_COLOR)
  base *= v_color;
#endif
  if(score_feat(m.feature_mask, SCORE_FEAT_ALPHA_MASK) && base.a < m.alpha_cutoff)
    discard;

  if(mode == MODE_WIREFRAME)
  {
    // Rasterised as lines
    if(lighting.wire.w > 0.5)
    {
      fragColor = vec4(lighting.wire.rgb, 1.0);
      return;
    }
    const float edge = min(min(v_bary.x, v_bary.y), abs(1.0 - v_bary.x - v_bary.y));
    fragColor = edge < 0.04 ? vec4(1.0) : vec4(25.0, 30.0, 40.0, 255.0) / 255.0;
    return;
  }
  if(mode == MODE_VERTEX_COLORS || (mode == MODE_POINTS && SCENE_HAS_COLOR))
  {
#if defined(SCENE_COLOR)
    fragColor = vec4(v_color.rgb, 1.0);
#else
    fragColor = vec4(encode(m.baseColor.rgb), 1.0);
#endif
    return;
  }
  if(mode == MODE_TEXCOORDS)
  {
#if defined(SCENE_UV)
    const ivec2 cell = ivec2(floor(v_uv * 8.0));
    fragColor = ((cell.x + cell.y) & 1) != 0 ? vec4(235.0, 235.0, 235.0, 255.0) / 255.0
                                             : vec4(70.0, 120.0, 200.0, 255.0) / 255.0;
#else
    fragColor = vec4(0.25, 0.25, 0.28, 1.0);
#endif
    return;
  }

  if(score_feat(m.feature_mask, SCORE_FEAT_UNLIT))
  {
    fragColor = vec4(encode(base.rgb), 1.0);
    return;
  }

#if defined(SCENE_NORMAL)
  vec3 N = normalize(v_normal);
#else
  vec3 N = normalize(cross(dFdx(v_world), dFdy(v_world)));
#endif
  const vec3 V = normalize(lighting.eye.xyz - v_world);
  // Two-sided: geometry comes with whatever winding it was authored with
  if(dot(N, V) < 0.0)
    N = -N;
  if(any(isnan(N)))
    N = V;

  ScoreBRDFInputs s;
  s.N = N;
  s.V = V;
  s.base = base.rgb;
  s.metallic = clamp(m.metallicRoughnessOcclusionUnlit.x, 0.0, 1.0);
  s.roughness = clamp(m.metallicRoughnessOcclusionUnlit.y, 0.04, 1.0);

  // A key light and a fill light, both turning with the camera, over a
  // little ambient
  vec3 color = score_brdf_fake_env(s) + 0.06 * base.rgb;
  for(int i = 0; i < 2; i++)
  {
    const vec4 light = i == 0 ? lighting.key : lighting.fill;
    const vec3 L = normalize(light.xyz);
    const float NoL = max(dot(N, L), 0.0);
    const ScoreBRDFEval e = score_brdf_eval(s, L);
    color += (e.diffuse + e.specular) * NoL * light.w;
  }
  // An emissive texture that cannot be sampled leaves the emission out
  // rather than lighting the whole surface with its factor
  vec3 emissive = m.emissive_strength.rgb * m.emissive_strength.w;
#if defined(SCENE_UV)
  if((textures & 2) != 0)
    emissive *= texture(emissive_tex, materialUV(v_uv)).rgb;
  else if(score_feat(m.feature_mask, SCORE_FEAT_HAS_EMISSIVE_TEX))
    emissive = vec3(0.0);
#else
  if(score_feat(m.feature_mask, SCORE_FEAT_HAS_EMISSIVE_TEX))
    emissive = vec3(0.0);
#endif
  color += emissive;
  fragColor = vec4(encode(color), 1.0);
}
)_";

//! The per-draw block of the shaders
struct DrawUBO
{
  float model[16];
  float normal[16];
  score::gfx::MaterialGPU material;
  float uv_offset_scale[4];
  //! Mode, the textures bound (1 base colour, 2 emissive), point size, uv
  //! rotation
  float params[4];
};
static_assert(sizeof(DrawUBO) == 240, "DrawUBO must match draw_t");

struct LightingUBO
{
  //! Position of the camera
  float eye[4];
  //! Direction towards the light, and its intensity
  float key[4];
  float fill[4];
  //! The colour of wireframe lines, w raised when lines are rasterised
  float wire[4];
};

uint64_t hashCombine(uint64_t seed, uint64_t v) noexcept
{
  return seed ^ (v + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
}

uint64_t bufferIdentity(const ossia::buffer_resource_ptr& b) noexcept
{
  if(!b)
    return 0;
  uint64_t h = hashCombine(uint64_t(reinterpret_cast<uintptr_t>(b.get())), uint64_t(b->dirty_index));
  h = hashCombine(h, b->content_hash);
  if(auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(&b->resource))
    h = hashCombine(h, uint64_t(reinterpret_cast<uintptr_t>(gpu->native_handle)));
  else if(auto* cpu = ossia::get_if<ossia::buffer_data>(&b->resource))
    h = hashCombine(h, uint64_t(reinterpret_cast<uintptr_t>(cpu->data.get())));
  return h;
}

//! The attributes of a geometry the shaders read
uint8_t attributesOf(const ossia::geometry& g) noexcept
{
  uint8_t bits = 0;
  for(const auto& a : g.attributes)
  {
    switch(a.semantic)
    {
      case ossia::attribute_semantic::normal:
        bits |= variant_normal;
        break;
      case ossia::attribute_semantic::texcoord0:
        bits |= variant_uv;
        break;
      case ossia::attribute_semantic::color0:
        bits |= variant_color;
        break;
      default:
        break;
    }
  }
  return bits;
}

uint64_t layoutOf(const ossia::geometry& g) noexcept
{
  uint64_t h = hashCombine(uint64_t(g.topology), uint64_t(g.index.buffer >= 0));
  for(const auto& b : g.bindings)
    h = hashCombine(hashCombine(h, uint64_t(b.byte_stride)), uint64_t(b.classification));
  for(const auto& a : g.attributes)
  {
    h = hashCombine(h, uint64_t(a.binding));
    h = hashCombine(h, uint64_t(a.format));
    h = hashCombine(h, uint64_t(a.byte_offset));
    h = hashCombine(h, uint64_t(a.semantic));
  }
  return h;
}

void toGL(const QMatrix4x4& m, float (&out)[16]) noexcept
{
  std::memcpy(out, m.constData(), sizeof(out));
}

//! One gizmo or box: line segments in the vertex format of the guides
struct LineWriter
{
  std::vector<float>& out;
  QMatrix4x4 transform;
  std::array<float, 4> color{1.f, 1.f, 1.f, 1.f};

  void segment(QVector3D a, QVector3D b)
  {
    for(const QVector3D& p : {a, b})
    {
      const QVector3D w = transform.map(p);
      out.insert(out.end(), {w.x(), w.y(), w.z(), color[0], color[1], color[2], color[3]});
    }
  }

  void box(const ossia::aabb& b)
  {
    const QVector3D lo{b.min[0], b.min[1], b.min[2]}, hi{b.max[0], b.max[1], b.max[2]};
    auto corner = [&](int i) {
      return QVector3D{
          (i & 1) ? hi.x() : lo.x(), (i & 2) ? hi.y() : lo.y(), (i & 4) ? hi.z() : lo.z()};
    };
    for(int i = 0; i < 8; i++)
      for(int axis : {1, 2, 4})
        if(!(i & axis))
          segment(corner(i), corner(i | axis));
  }
};

constexpr int line_vertex_floats = 7;
}

SceneDrawer::SceneDrawer(const GeometryViewerNode& node)
    : m_node{node}
{
}

SceneDrawer::~SceneDrawer()
{
  release();
}

bool SceneDrawer::setScene(const ossia::scene_spec& scene)
{
  const ossia::scene_state* state = scene.state.get();
  const int64_t version = state ? state->version : -1;
  const int64_t dirty = state ? state->dirty_index : -1;
  if(state == m_state && version == m_version && dirty == m_dirty)
    return false;
  m_scene = scene;
  m_state = state;
  m_version = version;
  m_dirty = dirty;
  walk();
  return true;
}

void SceneDrawer::walk()
{
  describeScene(m_scene, m_desc);

  for(auto& [k, m] : m_meshes)
    m.used = false;
  for(auto& [k, t] : m_textures)
    t.used = false;

  m_draws.clear();
  m_draws.reserve(m_desc.draws.size());
  const ossia::geometry_spec* only_legacy{};
  bool plain = true;
  for(const auto& item : m_desc.draws)
  {
    Draw d;
    d.world = item.world;
    d.material = item.material;
    if(item.primitive)
    {
      d.mesh = meshFor(*item.primitive);
      plain = false;
    }
    else
    {
      d.mesh = meshFor(*item.legacy, item.legacy_index);
      if(!item.world.isIdentity() || (only_legacy && only_legacy->meshes != item.legacy->meshes))
        plain = false;
      only_legacy = item.legacy;
    }
    if(d.mesh == 0)
      continue;
    if(d.material)
    {
      d.texture = textureFor(d.material->base_color_texture);
      d.emissive = textureFor(d.material->emissive_texture);
    }
    d.bindings = d.texture || d.emissive ? hashCombine(d.texture, d.emissive) : 0;
    m_draws.push_back(std::move(d));
  }

  // A single geometry without transforms, lights or cameras is what the
  // renderer draws on its own, filters included
  m_active = !(plain && m_desc.gizmos.empty() && m_desc.skipped_instances == 0)
             && (!m_draws.empty() || !m_desc.gizmos.empty());

  // What the scene no longer uses goes; its GPU mesh stays in the cache of
  // the RenderList until nothing else holds its geometry
  for(auto it = m_meshes.begin(); it != m_meshes.end();)
    it = it->second.used ? std::next(it) : m_meshes.erase(it);
  bool texturesGone = false;
  for(auto it = m_textures.begin(); it != m_textures.end();)
  {
    if(!it->second.used)
    {
      if(it->second.texture && !it->second.borrowed)
        it->second.texture->deleteLater();
      it = m_textures.erase(it);
      texturesGone = true;
    }
    else
      ++it;
  }
  // The bindings that referred to them
  if(texturesGone)
    for(auto& pass : m_passes)
    {
      for(auto& [k, b] : pass.bindings)
        b->deleteLater();
      pass.bindings.clear();
    }

  m_drawsDirty = true;
  m_linesDirty = true;
  m_passesDirty = true;
  // The rows a selection refers to changed
  m_serial++;
  m_summaryPending = true;
}

void SceneDrawer::publish()
{
  // Only while something shows it: a scene that changes every frame would
  // otherwise copy its summary every frame for nobody
  auto& monitor = m_node.monitor;
  if(!m_summaryPending || !monitor || !monitor->watched())
    return;
  auto summary = std::make_shared<SceneSummary>(m_desc.summary);
  summary->serial = m_serial;
  monitor->summary.produce(std::shared_ptr<const SceneSummary>(std::move(summary)));
  m_summaryPending = false;
}

uint64_t SceneDrawer::meshFor(const ossia::mesh_primitive& prim)
{
  uint64_t key = hashCombine(1, uint64_t(prim.topology));
  key = hashCombine(key, uint64_t(prim.index_type));
  key = hashCombine(key, prim.vertex_count);
  key = hashCombine(key, prim.index_count);
  key = hashCombine(key, prim.first_vertex);
  key = hashCombine(key, prim.first_index);
  key = hashCombine(key, uint64_t(uint32_t(prim.vertex_offset)));
  for(const auto& b : prim.vertex_buffers)
    key = hashCombine(key, bufferIdentity(b));
  key = hashCombine(key, bufferIdentity(prim.index_buffer));
  for(const auto& a : prim.attributes)
  {
    key = hashCombine(key, uint64_t(a.semantic));
    key = hashCombine(key, uint64_t(a.format));
    key = hashCombine(key, a.buffer_index);
    key = hashCombine(key, a.byte_offset);
    key = hashCombine(key, a.byte_stride);
    key = hashCombine(key, uint64_t(a.rate));
  }
  if(key == 0)
    key = 1;

  if(auto it = m_meshes.find(key); it != m_meshes.end())
  {
    it->second.used = true;
    return key;
  }

  auto geom = score::gfx::primitiveToGeometry(prim);
  if(!geom)
    return 0;
  MeshEntry e;
  e.attributes = attributesOf(*geom);
  e.layout = layoutOf(*geom);
  e.spec.meshes = std::make_shared<ossia::mesh_list>();
  e.spec.meshes->meshes.push_back(std::move(*geom));
  e.used = true;
  m_meshes.emplace(key, std::move(e));
  return key;
}

uint64_t SceneDrawer::meshFor(const ossia::geometry_spec& spec, std::size_t index)
{
  if(!spec.meshes || index >= spec.meshes->meshes.size())
    return 0;
  uint64_t key = hashCombine(2, uint64_t(reinterpret_cast<uintptr_t>(spec.meshes.get())));
  key = hashCombine(key, uint64_t(reinterpret_cast<uintptr_t>(spec.filters.get())));
  key = hashCombine(key, index);
  key = hashCombine(key, uint64_t(spec.meshes->dirty_index));
  if(key == 0)
    key = 2;

  if(auto it = m_meshes.find(key); it != m_meshes.end())
  {
    it->second.used = true;
    return key;
  }

  const auto& g = spec.meshes->meshes[index];
  MeshEntry e;
  e.attributes = attributesOf(g);
  e.layout = layoutOf(g);
  if(spec.meshes->meshes.size() == 1)
  {
    // The very geometry the producer published: the RenderList shares its
    // mesh with any renderer that draws it
    e.spec = spec;
    e.live = true;
  }
  else
  {
    e.spec.meshes = std::make_shared<ossia::mesh_list>();
    e.spec.meshes->meshes.push_back(g);
    e.spec.meshes->dirty_index = spec.meshes->dirty_index;
    e.spec.filters = spec.filters;
  }
  e.used = true;
  m_meshes.emplace(key, std::move(e));
  return key;
}

uint64_t SceneDrawer::textureFor(const ossia::texture_ref& ref)
{
  if(!ref.valid())
    return 0;
  uint64_t key{};
  bool borrowed = false;
  if(ref.texture.native_handle)
  {
    key = hashCombine(3, uint64_t(reinterpret_cast<uintptr_t>(ref.texture.native_handle)));
    borrowed = true;
  }
  else if(ref.source)
  {
    key = ref.source->content_hash != 0
              ? hashCombine(4, ref.source->content_hash)
              : hashCombine(5, uint64_t(reinterpret_cast<uintptr_t>(ref.source.get())));
  }
  else
    return 0;
  if(key == 0)
    key = 6;

  auto& t = m_textures[key];
  t.used = true;
  if(!t.source && !borrowed)
    t.source = ref.source;
  if(borrowed && !t.texture)
  {
    t.texture = static_cast<QRhiTexture*>(ref.texture.native_handle);
    t.borrowed = true;
  }
  return key;
}

void SceneDrawer::uploadTextures(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  auto& rhi = *renderer.state.rhi;
  for(auto& [key, t] : m_textures)
  {
    if(t.texture || t.failed || !t.source)
      continue;
    QImage img;
    const auto& src = *t.source;
    if(src.embedded_data && !src.embedded_data->empty())
      img.loadFromData(
          reinterpret_cast<const uchar*>(src.embedded_data->data()),
          int(src.embedded_data->size()));
    else if(!src.file_path.empty())
      img.load(QString::fromStdString(src.file_path));
    if(img.isNull())
    {
      // KTX2, Basis and the formats Qt cannot read: the base colour factor
      // alone
      t.failed = true;
      continue;
    }
    const int max = std::max(1, rhi.resourceLimit(QRhi::TextureSizeMax));
    if(img.width() > max || img.height() > max)
      img = img.scaled(max, max, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img = std::move(img).convertToFormat(QImage::Format_RGBA8888);

    // Colour data: sampled back to linear light
    auto* tex = rhi.newTexture(
        QRhiTexture::RGBA8, img.size(), 1,
        QRhiTexture::sRGB | QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
    tex->setName("GeometryViewer::base_color");
    if(!tex->create())
    {
      delete tex;
      t.failed = true;
      continue;
    }
    res.uploadTexture(tex, img);
    res.generateMips(tex);
    t.texture = tex;
  }
}

bool SceneDrawer::ensureShaders(score::gfx::RenderList& renderer, int variant)
{
  if(m_shaders[variant])
    return true;
  if(m_shaderFailed[variant])
    return false;
  QString defines = QStringLiteral("#version 450\n");
  if(variant & variant_normal)
    defines += QStringLiteral("#define SCENE_NORMAL 1\n");
  if(variant & variant_uv)
    defines += QStringLiteral("#define SCENE_UV 1\n");
  if(variant & variant_color)
    defines += QStringLiteral("#define SCENE_COLOR 1\n#define SCENE_HAS_COLOR true\n");
  else
    defines += QStringLiteral("#define SCENE_HAS_COLOR false\n");
  if(variant & variant_points)
    defines += QStringLiteral("#define SCENE_POINTS 1\n");

  const QString vs = defines + QString::fromUtf8(scene_vertex_shader);
  const QString fs = defines + QString::fromUtf8(scene_default_uniforms)
                     + QString::fromUtf8(Threedim::shaderlib::score_scene_common)
                     + QString::fromUtf8(Threedim::shaderlib::score_pbr_brdf)
                     + QString::fromUtf8(scene_fragment_shader);
  try
  {
    m_shaders[variant] = score::gfx::makeShaders(renderer.state, vs, fs);
    return true;
  }
  catch(const std::exception& e)
  {
    qWarning() << "Geometry viewer: the scene shaders did not compile:" << e.what();
    m_shaderFailed[variant] = true;
    return false;
  }
}

uint64_t SceneDrawer::pipelineKey(const MeshEntry& m) const noexcept
{
  return hashCombine(hashCombine(m.layout, m.attributes), uint64_t(m_mode));
}

score::gfx::Pipeline SceneDrawer::makePipeline(
    score::gfx::RenderList& renderer, const MeshEntry& m, const EdgePass& pass,
    QRhiShaderResourceBindings* srb)
{
  const auto* geom = m.mesh ? m.mesh->semanticGeometry() : nullptr;
  if(!geom || !m.mesh->hasGeometry())
    return {};
  const bool points = Mode(m_mode) == Mode::Points;
  const int variant = m.attributes | (points ? variant_points : 0);
  if(!ensureShaders(renderer, variant))
    return {};
  const auto& [vs, fs] = *m_shaders[variant];

  auto& rhi = *renderer.state.rhi;
  const auto& rt = pass.target;
  score::gfx::Pipeline ret;
  auto* ps = rhi.newGraphicsPipeline();
  ps->setName("GeometryViewer::scene");
  QRhiGraphicsPipeline::TargetBlend blend;
  ps->setTargetBlends({blend});
  const int samples = rt.sampleCount();
  ps->setSampleCount(samples > 0 ? samples : renderer.samples());
  m.mesh->preparePipeline(*ps);
  if(!score::gfx::remapPipelineVertexInputs(*ps, vs, *geom, &ret.plan, &rhi))
  {
    delete ps;
    return {};
  }
  if(points)
    ps->setTopology(QRhiGraphicsPipeline::Points);
  else if(
      Mode(m_mode) == Mode::Wireframe && rhi.isFeatureSupported(QRhi::NonFillPolygonMode)
      && ps->topology() == QRhiGraphicsPipeline::Triangles)
    ps->setPolygonMode(QRhiGraphicsPipeline::Line);
  // Both sides: the viewer shows geometry it does not control
  ps->setCullMode(QRhiGraphicsPipeline::None);
  const bool depth = rt.depthTexture || rt.depthRenderBuffer || rt.msDepthTexture;
  ps->setDepthTest(depth);
  ps->setDepthWrite(depth);
  ps->setDepthOp(QRhiGraphicsPipeline::Greater);
  ps->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
  ps->setShaderResourceBindings(srb);
  ps->setRenderPassDescriptor(rt.renderPass);
  if(!ps->create())
  {
    delete ps;
    return {};
  }
  // The bindings belong to the pass, which shares them between pipelines
  ret.pipeline = ps;
  return ret;
}

QRhiTexture* SceneDrawer::textureOf(uint64_t key) const noexcept
{
  if(key == 0)
    return nullptr;
  auto t = m_textures.find(key);
  return t != m_textures.end() ? t->second.texture : nullptr;
}

QRhiShaderResourceBindings* SceneDrawer::bindingsFor(
    score::gfx::RenderList& renderer, EdgePass& pass, const Draw& draw,
    QRhiBuffer* processUBO, QRhiBuffer* cameraUBO)
{
  if(auto it = pass.bindings.find(draw.bindings); it != pass.bindings.end())
    return it->second;
  QRhiTexture* base = textureOf(draw.texture);
  QRhiTexture* emissive = textureOf(draw.emissive);
  const auto stages
      = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
  QRhiShaderResourceBinding extra[3]{
      QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
          4, stages, m_drawUBO, sizeof(DrawUBO)),
      QRhiShaderResourceBinding::uniformBuffer(5, stages, m_lightingUBO),
      QRhiShaderResourceBinding::sampledTexture(
          6, stages, emissive ? emissive : &renderer.whiteTexture(), m_sampler)};
  const score::gfx::Sampler samplers[1]{
      {.sampler = m_sampler, .texture = base ? base : &renderer.whiteTexture()}};
  auto* srb = score::gfx::createDefaultBindings(
      renderer, pass.target, processUBO, cameraUBO, samplers, extra);
  pass.bindings[draw.bindings] = srb;
  return srb;
}

void SceneDrawer::uploadDraws(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, const SceneView& view)
{
  const int64_t count = std::max<int64_t>(1, int64_t(m_draws.size()));
  const auto uploadsAll = [&] {
    auto& data = m_drawData;
    data.assign(m_drawStride * count, 0);
    for(std::size_t i = 0; i < m_draws.size(); i++)
    {
      const auto& d = m_draws[i];
      DrawUBO u{};
      const QMatrix4x4 model = view.model * d.world;
      toGL(model, u.model);
      QMatrix4x4 normal{model.normalMatrix()};
      toGL(normal, u.normal);
      if(d.material)
        u.material = score::gfx::packMaterial(*d.material);
      const auto& tr = d.material ? d.material->base_color_texture.uv_transform
                                  : ossia::texture_ref::transform{};
      u.uv_offset_scale[0] = tr.offset[0];
      u.uv_offset_scale[1] = tr.offset[1];
      u.uv_offset_scale[2] = tr.scale[0];
      u.uv_offset_scale[3] = tr.scale[1];
      u.params[0] = float(m_mode);
      u.params[1] = float((textureOf(d.texture) ? 1 : 0) | (textureOf(d.emissive) ? 2 : 0));
      u.params[2] = 2.f;
      u.params[3] = tr.rotation;
      std::memcpy(data.data() + i * m_drawStride, &u, sizeof(u));
    }
    res.updateDynamicBuffer(m_drawUBO, 0, quint32(data.size()), data.data());
  };

  auto& rhi = *renderer.state.rhi;
  if(!m_drawUBO || m_drawCapacity < count)
  {
    // The bindings refer to the buffer they were made with
    releasePasses();
    m_passesDirty = true;
    if(m_drawUBO)
      m_drawUBO->deleteLater();
    m_drawStride = rhi.ubufAligned(sizeof(DrawUBO));
    m_drawCapacity = std::max<int64_t>(count, 16);
    m_drawUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, quint32(m_drawStride * m_drawCapacity));
    m_drawUBO->setName("GeometryViewer::draws");
    if(!m_drawUBO->create())
    {
      delete m_drawUBO;
      m_drawUBO = nullptr;
      m_drawCapacity = 0;
      return;
    }
    m_drawsDirty = true;
  }
  // The draws depend on the model matrix of the viewer, not on its camera
  if(m_drawsDirty || view.model != m_drawsModel)
  {
    uploadsAll();
    m_drawsDirty = false;
    m_drawsModel = view.model;
  }

  if(!m_lightingUBO)
  {
    m_lightingUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LightingUBO));
    m_lightingUBO->setName("GeometryViewer::lighting");
    if(!m_lightingUBO->create())
    {
      delete m_lightingUBO;
      m_lightingUBO = nullptr;
      return;
    }
  }
  {
    // The lights are fixed to the camera: in view space, key above right,
    // fill low on the left
    const QMatrix4x4 toWorld = view.view.inverted();
    const QVector3D key = toWorld.mapVector(QVector3D{0.45f, 0.65f, 0.6f}).normalized();
    const QVector3D fill = toWorld.mapVector(QVector3D{-0.7f, -0.15f, 0.45f}).normalized();
    const bool lines = rhi.isFeatureSupported(QRhi::NonFillPolygonMode);
    LightingUBO l{
        {view.eye.x(), view.eye.y(), view.eye.z(), 1.f},
        {key.x(), key.y(), key.z(), 2.8f},
        {fill.x(), fill.y(), fill.z(), 0.9f},
        {0.82f, 0.86f, 0.94f, lines ? 1.f : 0.f}};
    res.updateDynamicBuffer(m_lightingUBO, 0, sizeof(l), &l);
  }
}

void SceneDrawer::uploadLines(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, const SceneView& view)
{
  m_lineData.clear();
  const auto& b = m_desc.bounds;
  float radius = 1.f;
  if(!b.empty())
    radius = std::max(
        0.5f * QVector3D{b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2]}.length(),
        1e-3f);
  const float s = radius * 0.12f;

  LineWriter w{m_lineData, {}, {1.f, 1.f, 1.f, 1.f}};
  if(m_gizmos)
  {
    for(const auto& g : m_desc.gizmos)
    {
      w.transform = view.model * g.world;
      w.color = {g.color[0], g.color[1], g.color[2], 1.f};
      if(g.kind == SceneGizmo::Light)
      {
        // A star, and the direction it shines in, -Z, for the lights that
        // have one
        w.segment({-s, 0, 0}, {s, 0, 0});
        w.segment({0, -s, 0}, {0, s, 0});
        w.segment({0, 0, -s}, {0, 0, s});
        const float d = s * 0.58f;
        w.segment({-d, -d, -d}, {d, d, d});
        w.segment({-d, d, -d}, {d, -d, d});
        w.segment({d, -d, -d}, {-d, d, d});
        w.segment({d, d, -d}, {-d, -d, d});
        if(g.light == ossia::light_type::directional || g.light == ossia::light_type::spot)
        {
          const float len = s * 4.f, head = s * 0.8f;
          const QVector3D tip{0, 0, -len};
          w.segment({0, 0, 0}, tip);
          for(auto side : {QVector3D{head, 0, 0}, QVector3D{-head, 0, 0},
                           QVector3D{0, head, 0}, QVector3D{0, -head, 0}})
            w.segment(tip, tip + side + QVector3D{0, 0, head * 1.5f});
        }
      }
      else
      {
        // A frustum looking down -Z, with a mark on its top edge
        const float depth = s * 3.f;
        const float h = g.yfov > 0.f ? depth * std::tan(g.yfov * 0.5f) : s;
        const float hw = h * g.aspect;
        const QVector3D apex = g.yfov > 0.f ? QVector3D{} : QVector3D{0, 0, 0};
        const QVector3D c[4]{
            {-hw, -h, -depth}, {hw, -h, -depth}, {hw, h, -depth}, {-hw, h, -depth}};
        for(int i = 0; i < 4; i++)
        {
          w.segment(g.yfov > 0.f ? apex : QVector3D{c[i].x(), c[i].y(), 0}, c[i]);
          w.segment(c[i], c[(i + 1) % 4]);
        }
        w.segment(c[3], {0, h * 1.4f, -depth});
        w.segment(c[2], {0, h * 1.4f, -depth});
      }
    }
  }

  // The object the inspector selected
  if(m_selected >= 0 && uint32_t(m_selected >> 32) == m_serial)
  {
    const auto row = std::size_t(uint32_t(m_selected));
    if(row < m_desc.summary.rows.size())
    {
      auto box = m_desc.summary.rows[row].bounds;
      if(!box.empty())
      {
        // Something with a position and no extent: a box around it
        for(int k = 0; k < 3; k++)
          if(box.max[k] - box.min[k] < s)
          {
            const float mid = 0.5f * (box.max[k] + box.min[k]);
            box.min[k] = mid - s;
            box.max[k] = mid + s;
          }
        w.transform = view.model;
        w.color = {1.f, 0.62f, 0.15f, 1.f};
        w.box(box);
      }
    }
  }

  m_lineVertices = int(m_lineData.size() / line_vertex_floats);
  if(m_lineVertices == 0)
    return;
  auto& rhi = *renderer.state.rhi;
  const int64_t bytes = int64_t(m_lineData.size() * sizeof(float));
  if(!m_lineBuffer || m_lineCapacity < bytes)
  {
    if(m_lineBuffer)
      m_lineBuffer->deleteLater();
    m_lineCapacity = std::max<int64_t>(bytes, 4096);
    m_lineBuffer = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, quint32(m_lineCapacity));
    m_lineBuffer->setName("GeometryViewer::gizmos");
    if(!m_lineBuffer->create())
    {
      delete m_lineBuffer;
      m_lineBuffer = nullptr;
      m_lineCapacity = 0;
      m_lineVertices = 0;
      return;
    }
  }
  res.updateDynamicBuffer(m_lineBuffer, 0, quint32(bytes), m_lineData.data());
}

void SceneDrawer::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
    score::gfx::PassMap& passes, QRhiBuffer* processUBO, QRhiBuffer* cameraUBO,
    const SceneView& view)
{
  publish();
  if(!m_active)
  {
    m_lineVertices = 0;
    m_wantsLines = false;
    return;
  }
  auto& rhi = *renderer.state.rhi;

  if(const int mode = m_node.sceneMode(); mode != m_mode)
  {
    m_mode = mode;
    m_drawsDirty = true;
    for(auto& pass : m_passes)
    {
      for(auto& [k, p] : pass.pipelines)
        p.release();
      pass.pipelines.clear();
    }
    m_passesDirty = true;
  }
  if(m_node.sceneGizmos() != m_gizmos)
  {
    m_gizmos = m_node.sceneGizmos();
    m_linesDirty = true;
  }
  if(m_node.monitor)
    if(const int64_t sel = m_node.monitor->selected.load(std::memory_order_relaxed);
       sel != m_selected)
    {
      m_selected = sel;
      m_linesDirty = true;
    }
  m_wantsLines = (m_gizmos && !m_desc.gizmos.empty()) || m_selected >= 0;

  // Meshes the scene added since the previous frame; the geometries shared
  // with their producer follow their changes, as the single geometry does
  for(auto& [key, m] : m_meshes)
  {
    if(m.mesh && !m.live)
      continue;
    auto [mesh, bufs] = renderer.acquireMesh(m.spec, res, nullptr, {});
    if(mesh != m.mesh)
    {
      // A mesh of the same layout shares the pipelines already made
      m_passesDirty = true;
    }
    m.mesh = mesh;
    m.buffers = bufs;
  }

  if(!m_sampler)
  {
    m_sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
        QRhiSampler::Repeat, QRhiSampler::Repeat);
    m_sampler->setName("GeometryViewer::sampler");
    m_sampler->create();
  }
  uploadTextures(renderer, res);
  uploadDraws(renderer, res, view);
  if(m_linesDirty || view.model != m_linesModel)
  {
    m_linesModel = view.model;
    uploadLines(renderer, res, view);
    m_linesDirty = false;
  }

  if(!m_drawUBO || !m_lightingUBO)
    return;

  // The passes follow those of the renderer, on their render pass
  for(auto it = m_passes.begin(); it != m_passes.end();)
  {
    auto pass = ossia::find_if(passes, [&](const auto& p) { return p.first == it->edge; });
    if(pass == passes.end() || pass->second.renderTarget.renderPass != it->renderPass)
    {
      for(auto& [k, p] : it->pipelines)
        p.release();
      for(auto& [k, b] : it->bindings)
        b->deleteLater();
      it = m_passes.erase(it);
    }
    else
      ++it;
  }
  for(auto& [edge, pass] : passes)
  {
    if(!pass.renderTarget.renderPass)
      continue;
    if(ossia::any_of(m_passes, [e = edge](const EdgePass& p) { return p.edge == e; }))
      continue;
    m_passes.push_back(EdgePass{
        .edge = edge, .renderPass = pass.renderTarget.renderPass,
        .target = pass.renderTarget, .pipelines = {}, .bindings = {}});
    m_passesDirty = true;
  }

  // What the draws need, once after each change rather than every frame
  if(!m_passesDirty)
    return;
  m_passesDirty = false;
  for(auto& pass : m_passes)
  {
    for(const auto& d : m_draws)
    {
      auto m = m_meshes.find(d.mesh);
      if(m == m_meshes.end() || !m->second.mesh)
        continue;
      auto* srb = bindingsFor(renderer, pass, d, processUBO, cameraUBO);
      const uint64_t key = pipelineKey(m->second);
      if(!pass.pipelines.contains(key))
        pass.pipelines[key] = makePipeline(renderer, m->second, pass, srb);
    }
  }
}

void SceneDrawer::draw(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& cb, score::gfx::Edge& edge)
{
  auto pass = ossia::find_if(m_passes, [&](const EdgePass& p) { return p.edge == &edge; });
  if(pass == m_passes.end() || !m_drawUBO)
    return;
  const auto sz = renderer.renderSize(&edge);
  const QRhiViewport viewport(0, 0, sz.width(), sz.height());
  QRhiGraphicsPipeline* bound{};
  for(std::size_t i = 0; i < m_draws.size(); i++)
  {
    const auto& d = m_draws[i];
    auto m = m_meshes.find(d.mesh);
    if(m == m_meshes.end() || !m->second.mesh)
      continue;
    auto p = pass->pipelines.find(pipelineKey(m->second));
    if(p == pass->pipelines.end() || !p->second.pipeline)
      continue;
    auto b = pass->bindings.find(d.bindings);
    if(b == pass->bindings.end())
      continue;
    if(p->second.pipeline != bound)
    {
      cb.setGraphicsPipeline(p->second.pipeline);
      cb.setViewport(viewport);
      bound = p->second.pipeline;
    }
    const QRhiCommandBuffer::DynamicOffset offset{4, quint32(i * m_drawStride)};
    cb.setShaderResources(b->second, 1, &offset);
    score::gfx::drawMeshWithPlan(*m->second.mesh, m->second.buffers, cb, p->second.plan);
  }
}

void SceneDrawer::drawLines(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& cb, score::gfx::Edge& edge,
    QRhiGraphicsPipeline* lines, QRhiShaderResourceBindings* srb)
{
  if(!m_active || !lines || !srb || !m_lineBuffer || m_lineVertices == 0)
    return;
  const auto sz = renderer.renderSize(&edge);
  cb.setGraphicsPipeline(lines);
  cb.setShaderResources(srb);
  cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
  const QRhiCommandBuffer::VertexInput input{m_lineBuffer, 0};
  cb.setVertexInput(0, 1, &input);
  cb.draw(m_lineVertices);
}

void SceneDrawer::releasePasses()
{
  for(auto& pass : m_passes)
  {
    for(auto& [k, p] : pass.pipelines)
      p.release();
    for(auto& [k, b] : pass.bindings)
      b->deleteLater();
  }
  m_passes.clear();
}

void SceneDrawer::release()
{
  releasePasses();
  for(auto& [k, m] : m_meshes)
  {
    m.mesh = nullptr;
    m.buffers = {};
  }
  for(auto& [k, t] : m_textures)
  {
    if(t.texture && !t.borrowed)
      t.texture->deleteLater();
    if(!t.borrowed)
      t.texture = nullptr;
    t.failed = false;
  }
  for(QRhiResource* r : std::initializer_list<QRhiResource*>{
          m_drawUBO, m_lightingUBO, m_sampler, m_lineBuffer})
    if(r)
      r->deleteLater();
  m_drawUBO = nullptr;
  m_lightingUBO = nullptr;
  m_sampler = nullptr;
  m_lineBuffer = nullptr;
  m_drawCapacity = 0;
  m_lineCapacity = 0;
  m_lineVertices = 0;
  m_drawsDirty = true;
  m_linesDirty = true;
  m_passesDirty = true;
  m_shaders = {};
  m_shaderFailed = {};
}
}
