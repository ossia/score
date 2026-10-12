#include "ShaderCache.hpp"
#include <QHash>
#include <QRegularExpression>

#include <string>
#include <string_view>
#include <vector>

#include <Gfx/Graph/RenderState.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/mutex.hpp>

namespace score::gfx
{

// -----------------------------------------------------------------------------
// gl_ViewIndex -> VIEWINDEX lowering for the Direct3D targets.
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
//     box, where d3d12 therefore fails exactly like d3d11 and the failure does
//     not look like a multiview problem at all.
//
// The runtime already has the answer. Where QRhi reports no MultiView,
// RenderedRawRasterPipelineNode renders the N views as N passes and stamps the
// view of each into ProcessUBO::viewIndex, which libisf exposes to both stages
// as isf_process_uniforms.VIEWINDEX_. On those targets the view index is
// therefore a uniform we are already uploading, and the shader can just read
// it. It is not PASSINDEX: a PER_MIP multiview shader (prefilter_ggx) reads
// PASSINDEX as its mip level, and lowering the view onto the same uniform
// rendered every mip into face 0, with the content of face <mip>.
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

// Replace every gl_ViewIndex reference with the VIEWINDEX uniform. Both the
// `#define VIEW_INDEX int(uint(gl_ViewIndex))` and the wrapper main's
// `isf_ViewIndexVarying = uint(gl_ViewIndex);` are plain occurrences of the same
// token, so one substitution covers the macro, the wrapper, and any shader
// that spelled the built-in out itself. The GL_EXT_multiview require goes too:
// nothing references the extension afterwards.
static QByteArray lowerViewIndexToPassIndex(QByteArray src)
{
  src.replace("#extension GL_EXT_multiview : require\n", "");
  src.replace("gl_ViewIndex", "isf_process_uniforms.VIEWINDEX_");
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
  auto& cache = b.forStage(stage);
  if(auto it = cache.find(shader); it != cache.end())
    return it->second;

  // Rename the shader's own identifiers that the target language reserves.
  //
  // SPIRV-Cross renames a user identifier that collides with a GLSL builtin
  // or with its own list of target keywords, but not every name the target
  // reserves: on D3D a `float lerp` met the lerp() it emits for mix(), and
  // fxc refused `float lerp = lerp(...)`; on Metal a `float new` or a function
  // called `plus` reached the Metal compiler as is. The same shader works on
  // OpenGL and Vulkan. See renameReservedIdentifiers. The cache stays keyed on
  // the original source.
  // See the file header: on a D3D target below SM 6.1 the
  // SV_ViewID that gl_ViewIndex becomes cannot compile at all, so route the
  // view index through the VIEWINDEX uniform the N-pass fallback already
  // stamps. Keyed on the ORIGINAL source, which is correct: the baker is
  // already per-(api, version, multiViewCount), so each backend caches its
  // own bake of the same node.
  QByteArray source = renameReservedIdentifiers(shader, api);
  if(multiViewCount >= 2 && viewIndexNeedsPassIndexFallback(api, version, multiViewCount))
    source = lowerViewIndexToPassIndex(std::move(source));

  b.baker.setSourceString(source, stage);
  b.baker.setPerTargetCompilation(true);

  // FIXME serialize / deserialize
  QShader baked = b.baker.bake();
  auto res = cache.insert({shader, {std::move(baked), b.baker.errorMessage()}});
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
      baker.setGeneratedShaders({{QShader::HlslShader, version}});
#if defined(QT_SHADERTOOLS_HAS_HLSL_READONLY_SSBO_SRV)
      // readonly storage blocks become ByteAddressBuffer SRVs, which D3D11
      // binds at every stage (vertex SSBOs), and whose loads fxc accepts in
      // gradient operations (X3662) and barrier-controlling flow (X3663).
      // D3D11 only: qrhid3d12 does not understand the {tN, -2} binding.
      baker.setHlslReadOnlyStorageBuffersAsSrv(true);
#endif
      break;
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
  if(multiViewCount >= 2 && !viewIndexNeedsPassIndexFallback(api, version, multiViewCount))
    baker.setMultiViewCount(multiViewCount);
#endif
}
}

namespace score::gfx
{
namespace
{
//! The HLSL names SPIRV-Cross emits that are not also GLSL names.
//!
//! These are the only identifiers worth renaming here. A user identifier that
//! is a GLSL builtin's name (min, step, length...) or an HLSL keyword is
//! renamed by SPIRV-Cross itself (CompilerGLSL::replace_illegal_names, which
//! the HLSL backend also calls with its own keyword list) -- and it could not
//! be renamed in the GLSL source anyway, where the same token also spells the
//! builtin. The names below have no meaning in GLSL, so every occurrence of one
//! in a GLSL source is the author's: a variable, a function, a struct member
//! or a macro. Renaming every occurrence keeps the program the same.
//!
//! Restricted to what spirv_hlsl.cpp actually writes (intrinsic calls and the
//! vector / matrix type names of its declarations) and to the HLSL reserved
//! words, so that a resource or member name that could not collide -- say a
//! buffer called "dst" -- keeps its name in the reflection data.
const ossia::hash_set<std::string_view>& hlslOnlyNames()
{
  static const std::vector<std::string> storage = [] {
    std::vector<std::string> n{
        "lerp", "frac", "rsqrt", "saturate", "mad", "mul", "fmod", "atan2",
        "ddx", "ddy", "ddx_coarse", "ddx_fine", "ddy_coarse", "ddy_fine",
        "asuint", "asint", "asfloat", "asdouble", "countbits", "reversebits",
        "firstbitlow", "firstbithigh", "f32tof16", "f16tof32",
        "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync",
        "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync",
        "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync",
        "InterlockedAdd", "InterlockedAnd", "InterlockedOr", "InterlockedXor",
        "InterlockedMin", "InterlockedMax", "InterlockedExchange",
        "InterlockedCompareExchange", "EvaluateAttributeAtCentroid",
        "EvaluateAttributeAtSample", "EvaluateAttributeSnapped",
        "NonUniformResourceIndex",
        // HLSL reserved words that are neither GLSL keywords nor GLSL
        // reserved words, and that SPIRV-Cross's HLSL keyword list lacks:
        // fxc stops at `float2 new = ...` with "unexpected token 'new'".
        "auto", "catch", "char", "const_cast", "delete", "dynamic_cast",
        "explicit", "friend", "mutable", "new", "operator", "private",
        "protected", "reinterpret_cast", "static_cast", "throw", "try",
        "typename", "virtual"};
    for(const char* t : {"float", "int", "uint", "bool", "half", "double", "min16float",
                         "min16int", "min16uint", "int16_t", "uint16_t", "int64_t",
                         "uint64_t", "float16_t"})
      for(int r = 1; r <= 4; r++)
      {
        n.push_back(t + std::to_string(r));
        for(int c = 1; c <= 4; c++)
          n.push_back(t + std::to_string(r) + "x" + std::to_string(c));
      }
    return n;
  }();
  static const ossia::hash_set<std::string_view> names{storage.begin(), storage.end()};
  return names;
}

//! The names the Metal Shading Language reserves that SPIRV-Cross's MSL
//! backend does not rename (CompilerMSL::get_reserved_keyword_set covers the
//! Metal attribute keywords and the metal_stdlib macros, not C++ itself).
//!
//! - C++ keywords and alternative tokens that are not GLSL keywords or
//!   reserved words: a GLSL `float new` or `float or` is legal and reaches
//!   the Metal compiler as `expected unqualified-id`. `not` is a GLSL builtin
//!   (SPIRV-Cross renames it) and the GLSL reserved words (class, template,
//!   this, ...) cannot be identifiers in the first place.
//! - names `using namespace metal` brings in that a user function of the same
//!   name makes ambiguous: plus, minus, array, the <functional> objects...
//! - quad and operator, seen in the corpus.
const ossia::hash_set<std::string_view>& mslOnlyNames()
{
  static const ossia::hash_set<std::string_view> names{
      // C++ keywords
      "new", "delete", "throw", "try", "catch", "char", "char8_t", "char16_t",
      "char32_t", "wchar_t", "virtual", "explicit", "mutable", "friend",
      "private", "protected", "typename", "nullptr", "operator", "auto",
      "register", "decltype", "constexpr", "consteval", "constinit",
      "static_assert", "alignas", "alignof", "noexcept", "thread_local",
      "typeid", "static_cast", "dynamic_cast", "const_cast", "reinterpret_cast",
      "concept", "requires", "co_await", "co_return", "co_yield", "export",
      // alternative tokens
      "and", "or", "xor", "bitand", "bitor", "compl", "and_eq", "or_eq",
      "xor_eq", "not_eq",
      // metal namespace
      "quad", "array", "array_ref", "vec", "plus", "minus", "multiplies",
      "divides", "modulus", "negate", "equal_to", "not_equal_to", "greater",
      "less", "greater_equal", "less_equal", "logical_and", "logical_or",
      "logical_not", "bit_and", "bit_or", "bit_xor", "bit_not",
      "numeric_limits", "enable_if", "conditional"};
  return names;
}

constexpr bool isIdentStart(char c) noexcept
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
constexpr bool isIdentChar(char c) noexcept
{
  return isIdentStart(c) || (c >= '0' && c <= '9');
}

QByteArray renameIdentifiers(
    const QByteArray& src, const ossia::hash_set<std::string_view>& names,
    std::string_view prefix)
{
  QByteArray out;
  const qsizetype n = src.size();
  const char* const s = src.constData();
  qsizetype copied = 0; // src[copied, i) still has to be appended to out
  bool renamed = false;

  for(qsizetype i = 0; i < n;)
  {
    const char c = s[i];
    const char d = (i + 1 < n) ? s[i + 1] : '\0';
    if(c == '/' && d == '/')
    {
      // Line comment. A backslash-newline continues it, as in the preprocessor.
      i += 2;
      while(i < n && !(s[i] == '\n' && s[i - 1] != '\\'))
        ++i;
    }
    else if(c == '/' && d == '*')
    {
      i += 2;
      while(i + 1 < n && !(s[i] == '*' && s[i + 1] == '/'))
        ++i;
      i = std::min(i + 2, n);
    }
    else if(c == '"')
    {
      // Only #include / #line / #error / #pragma carry strings in GLSL.
      ++i;
      while(i < n && s[i] != '"' && s[i] != '\n')
        ++i;
      if(i < n)
        ++i;
    }
    else if(c >= '0' && c <= '9')
    {
      // A number, suffixes and exponents included: 1e5, 0x1fu, 2.0lf.
      while(i < n && (isIdentChar(s[i]) || s[i] == '.'))
        ++i;
    }
    else if(isIdentStart(c))
    {
      const qsizetype b = i;
      while(i < n && isIdentChar(s[i]))
        ++i;
      const std::string_view tok{s + b, std::size_t(i - b)};
      if(names.contains(tok))
      {
        if(!renamed)
        {
          out.reserve(n + 64);
          renamed = true;
        }
        out.append(s + copied, b - copied);
        out.append(prefix.data(), prefix.size());
        out.append(tok.data(), tok.size());
        copied = i;
      }
    }
    else
    {
      ++i;
    }
  }

  if(!renamed)
    return src;
  out.append(s + copied, n - copied);
  return out;
}
}

QByteArray renameHlslReservedIdentifiers(const QByteArray& src)
{
  return renameIdentifiers(src, hlslOnlyNames(), "isf_hlsl_");
}

QByteArray renameMslReservedIdentifiers(const QByteArray& src)
{
  return renameIdentifiers(src, mslOnlyNames(), "isf_msl_");
}

QByteArray renameReservedIdentifiers(const QByteArray& src, GraphicsApi api)
{
  switch(api)
  {
    case GraphicsApi::D3D11:
    case GraphicsApi::D3D12:
      return renameHlslReservedIdentifiers(src);
    case GraphicsApi::Metal:
      return renameMslReservedIdentifiers(src);
    default:
      return src;
  }
}
}
