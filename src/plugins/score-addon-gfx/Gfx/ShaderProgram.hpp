#pragma once
#include <ossia/detail/hash_map.hpp>
#include <score/tools/Debug.hpp>
#include <isf.hpp>

#include <verdigris>

#include <QtGui/private/qshader_p.h>
#include <QDebug>
#include <QString>

#include <optional>
#include <array>
namespace isf
{
struct descriptor;
}

namespace Gfx
{
struct ShaderProgram {
  ShaderProgram() = default;
  ~ShaderProgram() = default;
  ShaderProgram(const ShaderProgram&) = default;
  ShaderProgram(ShaderProgram&&) = default;
  ShaderProgram(const QString& vert, const QString& frag)
    : vertex{vert}
    , fragment{frag}
  {
  }

  ShaderProgram(const std::vector<QString>& vec)
  {
    SCORE_ASSERT(vec.size() == 2);
    fragment = vec[0];
    vertex = vec[1];
  }

  ShaderProgram(std::vector<QString>&& vec)
  {
    SCORE_ASSERT(vec.size() == 2);
    fragment = std::move(vec[0]);
    vertex = std::move(vec[1]);
  }

  ShaderProgram& operator=(const ShaderProgram&) = default;
  ShaderProgram& operator=(ShaderProgram&&) = default;


  QString vertex;
  QString fragment;

  struct MemberSpec {
    const QString name;
    const QString ShaderProgram::* pointer;
    const std::string_view language{};
  };

  static const inline std::array<MemberSpec, 2> specification{
    MemberSpec{QObject::tr("Fragment"), &ShaderProgram::fragment, "GLSL"},
    MemberSpec{QObject::tr("Vertex"), &ShaderProgram::vertex, "GLSL"},
  };

  friend QDebug& operator<<(QDebug& d, const ShaderProgram& sp) {
    return (d << sp.vertex << sp.fragment);
  }
  friend bool operator==(const ShaderProgram& lhs, const ShaderProgram& rhs) noexcept {
    return lhs.vertex == rhs.vertex && lhs.fragment == rhs.fragment;
  }
  friend bool operator!=(const ShaderProgram& lhs, const ShaderProgram& rhs) noexcept {
    return !(lhs == rhs);
  }

  friend bool operator==(const std::vector<QString>& lhs, const ShaderProgram& rhs) noexcept
  {
    SCORE_ASSERT(lhs.size() == 2);
    return lhs[0] == rhs.*(ShaderProgram::specification[0].pointer) && lhs[1] == rhs.*(ShaderProgram::specification[1].pointer);
  }
  friend bool operator!=(const std::vector<QString>& lhs, const ShaderProgram& rhs) noexcept
  {
    return !(lhs == rhs);
  }
};

ShaderProgram programFromFragmentShaderPath(const QString& fsFilename, QByteArray fsData);
}

namespace std
{
template<>
struct hash<Gfx::ShaderProgram>
{
  std::size_t operator()(const Gfx::ShaderProgram& program) const noexcept
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
struct ProcessedProgram : ShaderProgram {
  isf::descriptor descriptor;

  QShader compiledVertex;
  QShader compiledFragment;
};

struct ProgramCache
{
  static ProgramCache& instance() noexcept;
  std::pair<std::optional<ProcessedProgram>, QString> get(const ShaderProgram& program) noexcept;

  ossia::fast_hash_map<ShaderProgram, ProcessedProgram> programs;
};

}

Q_DECLARE_METATYPE(Gfx::ShaderProgram)
W_REGISTER_ARGTYPE(Gfx::ShaderProgram)
Q_DECLARE_METATYPE(Gfx::ProcessedProgram)
W_REGISTER_ARGTYPE(Gfx::ProcessedProgram)
