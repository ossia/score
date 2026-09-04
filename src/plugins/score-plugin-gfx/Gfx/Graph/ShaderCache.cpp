#include "ShaderCache.hpp"
#include <QRegularExpression>

#include <Gfx/Graph/RenderState.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/mutex.hpp>

namespace score::gfx
{

const std::pair<QShader, QString>& ShaderCache::get(
    GraphicsApi api, const QShaderVersion& version, const QByteArray& shader,
    QShader::Stage stage, int multiViewCount)
{
  static std::mutex mut;
  static ShaderCache self TS_GUARDED_BY(mut);

  std::lock_guard<std::mutex> m{mut};

  // The view count is part of the baker's identity, not just of one bake:
  // it is baked into the GLSL (`layout(num_views = N) in;`) and into the
  // SPIR-V, so the same source at two view counts is two different shaders.
  auto ver_it = ossia::find_if(self.m_bakers, [&](const auto& p) {
    return p->api == api && p->version == version
           && p->multiViewCount == multiViewCount;
  });
  Baker* bb{};
  if(ver_it == self.m_bakers.end())
  {
    self.m_bakers.push_back(std::make_unique<Baker>(api, version, multiViewCount));
    bb = self.m_bakers.back().get();
  }
  else
  {
    bb = ver_it->get();
  }

  Baker& b = *bb;
  if(auto it = b.shaders.find(shader); it != b.shaders.end())
    return it->second;

  // A20: on the D3D targets, refuse a shader whose own identifiers collide
  // with an HLSL intrinsic, and say which one.
  //
  // SPIRV-Cross rewrites GLSL builtins to their HLSL spellings -- fract() ->
  // frac(), mix() -> lerp(), and so on -- but does NOT rename the user's
  // variables. A shader declaring `float frac` therefore emits
  // `float frac = frac(...)`, which fxc rejects with a message about the
  // user's own line that mentions neither GLSL nor the rename. The author
  // sees a shader that works on OpenGL, Vulkan and Metal and fails only on
  // Direct3D, for a reason nothing in their source suggests.
  //
  // Renaming instead of rejecting would mean rewriting the GLSL before
  // QShaderBaker hands it to SPIRV-Cross, which needs real tokenisation to
  // avoid touching strings, comments, struct members and swizzles. Rejecting
  // costs one scan and tells the author exactly what to rename.
  if(b.api == GraphicsApi::D3D11 || b.api == GraphicsApi::D3D12)
  {
    if(auto bad = score::gfx::hlslIntrinsicCollision(shader); !bad.isEmpty())
    {
      static const QString err
          = QStringLiteral("This shader declares \"%1\", which is the name of "
                           "an HLSL intrinsic. SPIRV-Cross translates GLSL "
                           "builtins to their HLSL spellings but does not "
                           "rename your variables, so the Direct3D backends "
                           "would emit \"%1 = %1(...)\". Rename it.");
      auto& slot = b.shaders[shader];
      slot = {QShader{}, err.arg(QString::fromUtf8(bad))};
      return slot;
    }
  }

  b.baker.setSourceString(shader, stage);
  b.baker.setPerTargetCompilation(true);

  // FIXME serialize / deserialize
  QShader baked = b.baker.bake();
  auto res = b.shaders.insert({shader, {std::move(baked), b.baker.errorMessage()}});
  return res.first->second;
}

const std::pair<QShader, QString>& ShaderCache::get(
    const RenderState& v, const QByteArray& shader, QShader::Stage stage,
    int multiViewCount)
{
  return ShaderCache::get(v.api, v.version, shader, stage, multiViewCount);
}

ShaderCache::ShaderCache() { }

ShaderCache::Baker::Baker(
    GraphicsApi api, const QShaderVersion& version, int multiViewCount)
    : api{api}
    , version{version}
    , multiViewCount{multiViewCount}
{
  switch(api)
  {
    case GraphicsApi::Null:
      baker.setGeneratedShaders({{QShader::SpirvShader, version}});
      break;
    case GraphicsApi::OpenGL:
      baker.setGeneratedShaders({{QShader::GlslShader, version}});
      break;
    case GraphicsApi::Vulkan:
      baker.setGeneratedShaders({{QShader::SpirvShader, version}});
      break;
    case GraphicsApi::D3D11:
    case GraphicsApi::D3D12:
      baker.setGeneratedShaders({{QShader::HlslShader, version}});
      break;
    case GraphicsApi::Metal:
      baker.setGeneratedShaders({{QShader::MslShader, version}});
      break;
  }
  baker.setGeneratedShaderVariants({{}});

  // Mandatory for any shader using gl_ViewIndex: without it the GLSL target
  // refuses to translate (`ovr_multiview_view_count must be non-zero when
  // using GL_OVR_multiview2`) and the SPIR-V target has no num_views to
  // emit. QShaderBaker gained this in 6.7, the same release as the QRhi
  // multiview API.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  if(multiViewCount >= 2)
    baker.setMultiViewCount(multiViewCount);
#endif
}
}

namespace score::gfx
{
/**
 * @brief The identifier a shader declares that collides with an HLSL intrinsic.
 *
 * Empty when there is none. Only the names SPIRV-Cross actually emits when
 * translating a GLSL builtin are listed: an identifier is only dangerous
 * because the generated HLSL will also contain a CALL by that name. A wider
 * list -- every HLSL intrinsic in existence -- would reject shaders that
 * compile perfectly well, which is worse than the bug.
 */
QByteArray hlslIntrinsicCollision(const QByteArray& src) noexcept
{
  // GLSL builtin -> HLSL spelling, for the ones whose HLSL name differs and is
  // a plausible variable name. `frac` is the one seen in the wild
  // (isf-long-numeric.fs). Names identical in both languages (sin, cos, abs...)
  // are just as dangerous, so they are here too.
  static const QByteArrayList intrinsics{
      "frac",  "lerp",  "rsqrt", "saturate", "ddx",   "ddy",  "mad",
      "mul",   "dot",   "cross", "normalize", "length", "step", "smoothstep",
      "clamp", "min",   "max",   "abs",      "sign",  "floor", "ceil",
      "round", "trunc", "sqrt",  "pow",      "exp",   "log",   "sin",
      "cos",   "tan",   "atan",  "asin",     "acos",  "fmod",  "reflect",
      "refract", "transpose", "determinant", "distance", "faceforward"};

  // Declarations only: "<type> <name>" where name is followed by something a
  // declaration can be followed by. Matching bare occurrences would flag every
  // legitimate CALL to these functions, which is the overwhelming majority.
  static const QRegularExpression decl(
      QStringLiteral(R"(\b(?:float|double|int|uint|bool|half|)"
                     R"(vec[234]|[iub]vec[234]|dvec[234]|)"
                     R"(mat[234](?:x[234])?)\s+([A-Za-z_]\w*)\s*(?=[;,=\[)]))"));

  const QString text = QString::fromUtf8(src);
  auto it = decl.globalMatch(text);
  while(it.hasNext())
  {
    const auto name = it.next().captured(1).toUtf8();
    if(!intrinsics.contains(name))
      continue;

    // Declaring the name is not enough to break anything. The generated HLSL
    // only becomes ill-formed when it contains BOTH the declaration and a CALL
    // by that name -- `float frac = frac(...)`. A shader that declares
    // `float step` and never calls step() compiles perfectly well, and
    // rejecting it would be a worse bug than the one being prevented: several
    // shaders in the corpus do exactly that.
    static const QString callFmt = QStringLiteral(R"(\b%1\s*\()");
    const QRegularExpression call(callFmt.arg(QString::fromUtf8(name)));
    if(call.match(text).hasMatch())
      return name;
  }
  return {};
}
}
