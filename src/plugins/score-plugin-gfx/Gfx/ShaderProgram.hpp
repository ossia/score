#pragma once
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Hashes.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/hash.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QDebug>
#include <QObject>
#include <QString>
#include <QtGui/private/qshader_p.h>

#include <isf.hpp>
#include <score_plugin_gfx_export.h>

#include <array>
#include <optional>
#include <verdigris>
namespace isf
{
struct descriptor;
}

namespace Gfx
{
struct SCORE_PLUGIN_GFX_EXPORT ShaderSource
{
  using ProgramType = isf::parser::ShaderType;
  ShaderSource() = default;
  ~ShaderSource() = default;
  ShaderSource(const ShaderSource&) = default;
  ShaderSource(ShaderSource&&) = default;

  ShaderSource(const QString& vert, const QString& frag)
      : type{isf::parser::ShaderType::ISF}
      , vertex{vert}
      , fragment{frag}
  {
  }

  ShaderSource(const std::vector<QString>& vec)
  {
    SCORE_ASSERT(vec.size() == 2);
    type = isf::parser::ShaderType::ISF;
    fragment = vec[0];
    vertex = vec[1];
  }
  ShaderSource(ProgramType tp, const QString& vert, const QString& frag)
      : type{tp}
      , vertex{vert}
      , fragment{frag}
  {
  }

  ShaderSource(ProgramType tp, const std::vector<QString>& vec)
  {
    SCORE_ASSERT(vec.size() == 2);
    type = tp;
    fragment = vec[0];
    vertex = vec[1];
  }

  ShaderSource(ProgramType tp, std::vector<QString>&& vec)
  {
    SCORE_ASSERT(vec.size() == 2);
    type = tp;
    fragment = std::move(vec[0]);
    vertex = std::move(vec[1]);
  }

  ShaderSource& operator=(const ShaderSource&) = default;
  ShaderSource& operator=(ShaderSource&&) = default;

  ProgramType type{};
  QString vertex;
  QString fragment;

  struct MemberSpec
  {
    const QString name;
    const QString ShaderSource::*pointer;
    const std::string_view language{};
  };

  static const inline std::array<MemberSpec, 2> specification{
      MemberSpec{QObject::tr("Fragment"), &ShaderSource::fragment, "GLSL"},
      MemberSpec{QObject::tr("Vertex"), &ShaderSource::vertex, "GLSL"},
  };

  friend QDebug& operator<<(QDebug& d, const ShaderSource& sp)
  {
    return (d << sp.vertex << sp.fragment);
  }
  friend bool operator==(const ShaderSource& lhs, const ShaderSource& rhs) noexcept
  {
    return lhs.vertex == rhs.vertex && lhs.fragment == rhs.fragment;
  }
  friend bool operator!=(const ShaderSource& lhs, const ShaderSource& rhs) noexcept
  {
    return !(lhs == rhs);
  }

  friend bool
  operator==(const std::vector<QString>& lhs, const ShaderSource& rhs) noexcept
  {
    SCORE_ASSERT(lhs.size() == 2);
    return lhs[0] == rhs.*(ShaderSource::specification[0].pointer)
           && lhs[1] == rhs.*(ShaderSource::specification[1].pointer);
  }
  friend bool
  operator!=(const std::vector<QString>& lhs, const ShaderSource& rhs) noexcept
  {
    return !(lhs == rhs);
  }
};

ShaderSource programFromISFFragmentShaderPath(const QString& fsFilename, QByteArray fsData);
ShaderSource
programFromVSAVertexShaderPath(const QString& vertexFilename, QByteArray vertexData);

// Textual `#include` resolution for a single GLSL buffer. Used by
// callers that want include support without going through the full
// ProgramCache ISF pipeline — compute shaders are the current use case.
// Returns the expanded source and a non-empty error string on failure
// (missing header, include cycle, depth limit, …). The returned
// QByteArray is empty iff the error is non-empty.
SCORE_PLUGIN_GFX_EXPORT
std::pair<QByteArray, QString>
preprocessShaderIncludes(QByteArray source, const QString& originPath = {}) noexcept;
}

namespace std
{
template <>
struct hash<Gfx::ShaderSource>
{
  std::size_t operator()(const Gfx::ShaderSource& program) const noexcept
  {
    // rapidhash via the gfx Qt-aware adapters; same primitive that
    // produces content_hash values throughout the gfx pipeline.
    std::size_t seed{(std::size_t)program.type};
    ossia::hash_combine(seed, score::gfx::hash_qstring(program.vertex));
    ossia::hash_combine(seed, score::gfx::hash_qstring(program.fragment));
    return seed;
  }
};
}

namespace Gfx
{
struct ProcessedProgram : ShaderSource
{
  isf::descriptor descriptor;
};

// Cache key. `originDir` is the *canonical directory* the shader was
// loaded from (derived by the cache from the caller-supplied origin
// path). Keying on both means two models loading the same source text
// from different directories don't collide — include resolution against
// each shader's own sibling dir stays correct.
struct ProgramCacheKey
{
  ShaderSource source;
  QString originDir;

  friend bool
  operator==(const ProgramCacheKey& a, const ProgramCacheKey& b) noexcept
  {
    return a.source == b.source && a.originDir == b.originDir;
  }
};
}

namespace std
{
template <>
struct hash<Gfx::ProgramCacheKey>
{
  std::size_t operator()(const Gfx::ProgramCacheKey& k) const noexcept
  {
    std::size_t seed = std::hash<Gfx::ShaderSource>{}(k.source);
    ossia::hash_combine(seed, score::gfx::hash_qstring(k.originDir));
    return seed;
  }
};
}

namespace Gfx
{
struct SCORE_PLUGIN_GFX_EXPORT ProgramCache
{
  static ProgramCache& instance() noexcept;

  // `originPath` is the absolute path of the shader file the source was
  // loaded from, used as the base for quoted `#include "..."` resolution
  // and as part of the cache key. Empty when the source is in-memory
  // with no associated file.
  std::pair<std::optional<ProcessedProgram>, QString>
  get(const ShaderSource& program, const QString& originPath = {}) noexcept;

  ossia::hash_map<ProgramCacheKey, ProcessedProgram> programs;
};

}

Q_DECLARE_METATYPE(Gfx::ShaderSource)
W_REGISTER_ARGTYPE(Gfx::ShaderSource)
Q_DECLARE_METATYPE(Gfx::ProcessedProgram)
W_REGISTER_ARGTYPE(Gfx::ProcessedProgram)
