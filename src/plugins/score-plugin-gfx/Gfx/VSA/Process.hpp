#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/ShaderProgram.hpp>
#include <Gfx/VSA/Metadata.hpp>

#include <isf.hpp>

namespace Gfx
{
struct ISFHelpers;
};
namespace Gfx::VSA
{

class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::VSA::Model)
  W_OBJECT(Model)
  friend struct Gfx::ISFHelpers;
  using Process::ProcessModel::m_inlets;
  using Process::ProcessModel::m_outlets;

public:
  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);
  Model(
      const TimeVal& duration, const QString& init, const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  bool validate(const QString& txt) const noexcept;

  const QString& vertex() const noexcept { return m_program.vertex; }
  Process::ScriptChangeResult setVertex(QString f);
  void vertexChanged(const QString& v) W_SIGNAL(vertexChanged, v);
  PROPERTY(QString, vertex READ vertex WRITE setVertex NOTIFY vertexChanged)

  void programChanged() W_SIGNAL(programChanged);

  const ProcessedProgram& processedProgram() const noexcept
  {
    return m_processedProgram;
  }

  void errorMessage(int line, const QString& arg_2) const
      W_SIGNAL(errorMessage, line, arg_2);

private:
  [[nodiscard]] Process::ScriptChangeResult setProgram(ShaderSource f);
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;

  QString prettyName() const noexcept override;

  ShaderSource m_program;
  ProcessedProgram m_processedProgram;
};

struct ProcessFactory final : Process::ProcessFactory_T<Gfx::VSA::Model>
{
public:
  Process::Descriptor descriptor(QString) const noexcept override;
};
}

#include <Scenario/Commands/ScriptEditCommand.hpp>

namespace Gfx
{
class ChangeVSAShader : public Scenario::EditScript<VSA::Model, VSA::Model::p_vertex>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeVSAShader, "Edit a script")
public:
  using EditScript::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<Gfx::VSA::Model::p_vertex> : Gfx::ChangeVSAShader
{
  using ChangeVSAShader::ChangeVSAShader;
};
}
