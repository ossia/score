#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <isf.hpp>

#include <array>
namespace Gfx
{
struct ISFHelpers;
};

namespace Gfx::CSF
{
class Model;
}

#include <Gfx/CSF/Metadata.hpp>

namespace Gfx::CSF
{

class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::CSF::Model)
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

  // FIXME ugh
  const QString& compute() const noexcept { return m_compute; }

  [[nodiscard]]
  Process::ScriptChangeResult setCompute(QString f);
  void computeChanged(const QString& f) W_SIGNAL(computeChanged, f);
  PROPERTY(QString, compute READ compute WRITE setCompute NOTIFY computeChanged)

  const isf::descriptor& descriptor() const noexcept
  {
    return m_processedProgram.descriptor;
  }

  const QString& processedCompute() const noexcept
  {
    return m_processedProgram.fragment;
  }

  [[nodiscard]] Process::ScriptChangeResult setScript(const QString& script);
  const QString& script() const noexcept { return m_compute; }
  void programChanged() W_SIGNAL(programChanged);

  void errorMessage(int line, const QString& err) const
      W_SIGNAL(errorMessage, line, err);

private:
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  QString prettyName() const noexcept override;

  void setupCSF(const isf::descriptor& desc);

  QString m_compute;
  ProcessedProgram m_processedProgram;
};

struct ProcessFactory final : Process::ProcessFactory_T<Gfx::CSF::Model>
{
public:
  Process::Descriptor descriptor(QString) const noexcept override;
};
}

#include <Scenario/Commands/ScriptEditCommand.hpp>

namespace Gfx
{
class ChangeCSF : public Scenario::EditScript<CSF::Model, CSF::Model::p_compute>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeCSF, "Edit CSF compute shader")
public:
  using EditScript::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<Gfx::CSF::Model::p_compute> : Gfx::ChangeCSF
{
  using ChangeCSF::ChangeCSF;
};
}
