#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/ShaderProgram.hpp>
#include <Library/LibraryInterface.hpp>

#include <score/command/PropertyCommand.hpp>

#include <Threedim/RenderPipeline/Metadata.hpp>

#include <isf.hpp>
namespace Gfx
{
struct ISFHelpers;
};
namespace Gfx::RenderPipeline
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::RenderPipeline::Model)
  W_OBJECT(Model)
  friend struct Gfx::ISFHelpers;
  using Process::ProcessModel::m_inlets;
  using Process::ProcessModel::m_outlets;

public:
  static constexpr bool hasExternalUI() noexcept { return true; }

  Model(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  ~Model() override;

  bool validate(const std::vector<QString>& txt) const noexcept;

  const QString& vertex() const noexcept { return m_program.vertex; }
  void setVertex(QString f);
  void vertexChanged(const QString& v) W_SIGNAL(vertexChanged, v);
  PROPERTY(QString, vertex READ vertex WRITE setVertex NOTIFY vertexChanged)

  const QString& fragment() const noexcept { return m_program.fragment; }
  void setFragment(QString f);
  void fragmentChanged(const QString& f) W_SIGNAL(fragmentChanged, f);
  PROPERTY(QString, fragment READ fragment WRITE setFragment NOTIFY fragmentChanged)

  const ShaderSource& program() const noexcept { return m_program; }
  [[nodiscard]] Process::ScriptChangeResult setProgram(ShaderSource f);
  void programChanged() W_SIGNAL(programChanged);
  PROPERTY(
      Gfx::ShaderSource, program READ program WRITE setProgram NOTIFY programChanged)

  const ProcessedProgram& processedProgram() const noexcept
  {
    return m_processedProgram;
  }

  void errorMessage(const QString& arg_2) const W_SIGNAL(errorMessage, arg_2);

private:
  void init();
  void initDefaultPorts();
  QString prettyName() const noexcept override;
  ShaderSource m_program;
  ProcessedProgram m_processedProgram;
};

struct ProcessFactory final : Process::ProcessFactory_T<Gfx::RenderPipeline::Model>
{
public:
  Process::Descriptor descriptor(QString) const noexcept override;
};
}

#include <Scenario/Commands/ScriptEditCommand.hpp>

namespace Gfx
{
class ChangeRenderPipelineShader
    : public Scenario::EditScript<
          RenderPipeline::Model, RenderPipeline::Model::p_program>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeRenderPipelineShader, "Edit a script")
public:
  using EditScript::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<Gfx::RenderPipeline::Model::p_program>
    : Gfx::ChangeRenderPipelineShader
{
  using ChangeRenderPipelineShader::ChangeRenderPipelineShader;
};
}
