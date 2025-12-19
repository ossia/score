#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/GeometryFilter/Metadata.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <isf.hpp>

namespace Gfx::GeometryFilter
{
struct ProcessedGeometryProgram
{
  isf::descriptor descriptor;
  QString shader;
};

class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::GeometryFilter::Model)
  W_OBJECT(Model)

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

  const QString& script() const noexcept { return m_script; }
  [[nodiscard]] Process::ScriptChangeResult setScript(const QString& f);
  void scriptChanged(const QString& arg_1) W_SIGNAL(scriptChanged, arg_1);
  void programChanged() W_SIGNAL(programChanged);
  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)

  const ProcessedGeometryProgram& processedProgram() const noexcept
  {
    return m_processedProgram;
  }

  void errorMessage(int arg_1, const QString& arg_2) const
      W_SIGNAL(errorMessage, arg_1, arg_2);

private:
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  void setupIsf(const isf::descriptor& d);
  //void setupNormalShader();
  QString prettyName() const noexcept override;

  QString m_script;
  ProcessedGeometryProgram m_processedProgram;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::GeometryFilter::Model>;

}

#include <Scenario/Commands/ScriptEditCommand.hpp>

namespace Gfx
{
class ChangeGeometryShader
    : public Scenario::EditScript<GeometryFilter::Model, GeometryFilter::Model::p_script>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeGeometryShader, "Edit a script")
public:
  using EditScript::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<Gfx::GeometryFilter::Model::p_script>
    : Gfx::ChangeGeometryShader
{
  using ChangeGeometryShader::ChangeGeometryShader;
};
}
