#pragma once
#include <Gfx/Graph/RenderState.hpp>

#include <score/tools/Debug.hpp>

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
}

namespace std
{
template <>
struct hash<Gfx::ShaderSource>
{
  std::size_t operator()(const Gfx::ShaderSource& program) const noexcept
  {
    constexpr const QtPrivate::QHashCombine combine;
    std::size_t seed{};
    seed = combine(seed, program.vertex);
    seed = combine(seed, program.fragment);
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

struct SCORE_PLUGIN_GFX_EXPORT ProgramCache
{
  static ProgramCache& instance() noexcept;
  std::pair<std::optional<ProcessedProgram>, QString>
  get(const ShaderSource& program) noexcept;

  ossia::hash_map<ShaderSource, ProcessedProgram> programs;
};

}

Q_DECLARE_METATYPE(Gfx::ShaderSource)
W_REGISTER_ARGTYPE(Gfx::ShaderSource)
Q_DECLARE_METATYPE(Gfx::ProcessedProgram)
W_REGISTER_ARGTYPE(Gfx::ProcessedProgram)
