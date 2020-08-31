#pragma once
#include <ossia/detail/hash_map.hpp>
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
  QString vertex;
  QString fragment;

  struct MemberSpec {
    QString name;
    QString ShaderProgram::* pointer;
  };

  static const inline std::array<MemberSpec, 2> specification{
    MemberSpec{QObject::tr("Fragment"), &ShaderProgram::fragment},
    MemberSpec{QObject::tr("Vertex"), &ShaderProgram::vertex},
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
};
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
  std::optional<ProcessedProgram> get(const ShaderProgram& program) noexcept;

  ossia::fast_hash_map<ShaderProgram, ProcessedProgram> programs;
};

}

Q_DECLARE_METATYPE(Gfx::ShaderProgram)
W_REGISTER_ARGTYPE(Gfx::ShaderProgram)
Q_DECLARE_METATYPE(Gfx::ProcessedProgram)
W_REGISTER_ARGTYPE(Gfx::ProcessedProgram)
