#pragma once
#include <Library/LibraryInterface.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/Filter/Metadata.hpp>
#include <isf.hpp>
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
};
}

Q_DECLARE_METATYPE(Gfx::ShaderProgram)
W_REGISTER_ARGTYPE(Gfx::ShaderProgram)
namespace Gfx::Filter
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Filter::Model)
  W_OBJECT(Model)

public:
  static constexpr bool hasExternalUI() noexcept { return true; }

  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  const QString& vertex() const noexcept { return m_program.vertex; }
  void setVertex(QString f);
  void vertexChanged(const QString& v) W_SIGNAL(vertexChanged, v);
  PROPERTY(QString, vertex READ vertex WRITE setVertex NOTIFY vertexChanged)

  const QString& fragment() const noexcept { return m_program.fragment; }
  void setFragment(QString f);
  void fragmentChanged(const QString& f) W_SIGNAL(fragmentChanged, f);
  PROPERTY(QString, fragment READ fragment WRITE setFragment NOTIFY fragmentChanged)

  const ShaderProgram& program() const noexcept { return m_program; }
  void setProgram(const ShaderProgram& f);
  void programChanged(const ShaderProgram& f) W_SIGNAL(programChanged, f);
  PROPERTY(Gfx::ShaderProgram, program READ program WRITE setProgram NOTIFY programChanged)

  const QString& processedVertex() const noexcept { return m_processedProgram.vertex; }
  const QString& processedFragment() const noexcept { return m_processedProgram.fragment; }
  const ShaderProgram& processedProgram() const noexcept { return m_processedProgram; }

  const isf::descriptor& isfDescriptor() const noexcept { return m_isfDescriptor; }
  void errorMessage(int arg_1, const QString& arg_2) W_SIGNAL(errorMessage, arg_1, arg_2);

private:
  void setupIsf(const isf::descriptor& d);
  void setupNormalShader();
  QString prettyName() const noexcept override;
  void startExecution() override;
  void stopExecution() override;
  void reset() override;

  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  ShaderProgram m_program;
  ShaderProgram m_processedProgram;
  isf::descriptor m_isfDescriptor;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::Filter::Model>;

class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("e62ed6f6-a2c1-4d27-a9c3-1c3bc576bfeb")

  QSet<QString> acceptedFiles() const noexcept override;
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("d1e16bba-4c53-4d24-8b6b-71b94daef68d")

  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  std::vector<ProcessDrop> dropData(
      const std::vector<DroppedFile>& data,
      const score::DocumentContext& ctx) const noexcept override;
};

}

#include <Scenario/Commands/ScriptEditCommand.hpp>

namespace Gfx
{
class ChangeShader : public Scenario::EditScript<Filter::Model, Filter::Model::p_program>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeShader, "Edit a script")
public:
  using EditScript::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<Gfx::Filter::Model::p_program> : Gfx::ChangeShader
{
  using ChangeShader::ChangeShader;
};
}
