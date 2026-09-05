#include "ShaderCache.hpp"
#include <QHash>
#include <QRegularExpression>

#include <Gfx/Graph/RenderState.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/mutex.hpp>

namespace score::gfx
{

// -----------------------------------------------------------------------------
// gl_ViewIndex -> PASSINDEX lowering for the Direct3D targets.
//
// libisf expands the VIEW_INDEX macro to gl_ViewIndex in the vertex stage
// (isf.cpp), and SPIRV-Cross translates that to HLSL's SV_ViewID, which needs
// shader model 6.1:
//
//     Vertex shader error: View Index input is only supported in VS and PS
//     6.1 or higher.
//
// That is fatal on both D3D backends we can actually reach:
//
//   * D3D11 is pinned to SM 5.0 for good -- fxc caps at 5.1 and
//     qrhid3d11.cpp looks up exactly {HlslShader, 50}. It can NEVER have
//     SV_ViewID.
//   * D3D12 asks for 6.1, but only when dxcompiler.dll is present; without it
//     d3d12ShaderVersion() falls back to 5.0 and lands in the same place. That
//     DLL is not on every machine -- it is absent from our own Windows test
//     box, which is why d3d12 failed identically to d3d11 and made the
//     failure look like something other than multiview.
//
// The runtime already has the answer. Where QRhi reports no MultiView,
// RenderedRawRasterPipelineNode renders the N views as N passes and stamps the
// invocation index into ProcessUBO::passIndex -- which libisf already exposes
// to both stages as PASSINDEX. On those targets the view index is therefore a
// uniform we are already uploading, and the shader can just read it.
//
// This is the same sidestep libisf documents for gl_NumWorkGroups, whose
// built-in SPIRV-Cross also refuses to emit on HLSL: route the reference
// through a uniform and textually shadow the built-in.
//
// Rewriting here rather than in libisf is deliberate: the shader string is
// generated once per NODE and shared by every renderer, while this decision
// belongs to the TARGET. ShaderCache is already partitioned by
// (api, version, multiViewCount), so the rewrite is cached per-backend and the
// OpenGL/Vulkan bakes of the same node keep real multiview.
// The predicate lives in RenderState.hpp as
// viewIndexNeedsPassIndexFallback(): the render path must agree with this
// rewrite, so both ask the same function.

// Replace every gl_ViewIndex reference with the PASSINDEX uniform. Both the
// `#define VIEW_INDEX gl_ViewIndex` and the wrapper main's
// `isf_ViewIndexVarying = gl_ViewIndex;` are plain occurrences of the same
// token, so one substitution covers the macro, the wrapper, and any shader
// that spelled the built-in out itself. The GL_EXT_multiview require goes too:
// nothing references the extension afterwards.
static QByteArray lowerViewIndexToPassIndex(QByteArray src)
{
  src.replace("#extension GL_EXT_multiview : require\n", "");
  src.replace("gl_ViewIndex", "isf_process_uniforms.PASSINDEX_");
  return src;
}


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

  // See viewIndexNeedsLowering() above: on a D3D target below SM 6.1 the
  // SV_ViewID that gl_ViewIndex becomes cannot compile at all, so route the
  // view index through the PASSINDEX uniform the N-pass fallback already
  // stamps. Keyed on the ORIGINAL source, which is correct: the baker is
  // already per-(api, version, multiViewCount), so each backend caches its
  // own bake of the same node.
  QByteArray source = shader;
  if(multiViewCount >= 2 && viewIndexNeedsPassIndexFallback(api, version))
    source = lowerViewIndexToPassIndex(std::move(source));

  b.baker.setSourceString(source, stage);
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
  // Not when the view index has been lowered to a uniform: there is no
  // gl_ViewIndex left to give a view count to, the target cannot express
  // multiview anyway, and asking for it makes QRhi expect a multiview render
  // target the N-pass fallback does not build.
  if(multiViewCount >= 2 && !viewIndexNeedsPassIndexFallback(api, version))
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
    // that SPIRV-Cross will spell with that name. A shader that declares
    // `float step` and never calls step() compiles perfectly well, and
    // rejecting it would be a worse bug than the one being prevented: several
    // shaders in the corpus do exactly that.
    //
    // The call must be looked for under its GLSL spelling, not its HLSL one.
    // This is what made the check miss the very shader its comment cites:
    // isf-long-numeric.fs declares `float frac` and calls `fract()`, and
    // `\bfrac\s*\(` does not match `fract(` -- "frac" there is followed by
    // "t", not "(". The collision only comes into existence when SPIRV-Cross
    // renames fract -> frac, by which point this source has long been read. So
    // the shader sailed past the guard and died in fxc instead, with
    //     error X3005: 'frac': identifier represents a variable, not a function
    // and a bare "Pipeline not created" -- the exact unactionable failure this
    // function was written to prevent.
    static const QHash<QByteArray, QByteArray> glslSpelling{
        {"frac", "fract"}, {"lerp", "mix"},   {"rsqrt", "inversesqrt"},
        {"ddx", "dFdx"},   {"ddy", "dFdy"},   {"fmod", "mod"},
        {"mad", "fma"}};
    QByteArrayList callNames{name};
    if(auto it = glslSpelling.find(name); it != glslSpelling.end())
      callNames.push_back(it.value());

    static const QString callFmt = QStringLiteral(R"(\b%1\s*\()");
    for(const auto& cn : callNames)
    {
      const QRegularExpression call(callFmt.arg(QString::fromUtf8(cn)));
      if(call.match(text).hasMatch())
        return name;
    }
  }
  return {};
}
}
