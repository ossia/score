#pragma once
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/Script/ScriptEditor.hpp>

#include <Scenario/Commands/ScriptEditCommand.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <JitCpp/EditScript.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>

#include <score_plugin_jit_export.h>
namespace AvndJit
{
class Model;
}

namespace Jit
{
struct Driver;
}
PROCESS_METADATA(
    , AvndJit::Model, "193686f2-1f3b-45ce-9251-2e79ca06933b", "Jit", "Avendish",
    Process::ProcessCategory::Script, "Script", "Avendish compilation process",
    "ossia score", QStringList{}, {}, {},
    QUrl(
        "https://ossia.io/score-docs/development/plugins/"
        "plugins-with-avendish.html#avendish-documentation"),
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::DynamicPorts)
namespace AvndJit
{
using NodeCompiler = Jit::Driver;
using NodeFunction = ossia::graph_node*(int, double);
using NodeFactory = std::function<NodeFunction>;
class Model : public Process::ProcessModel
{
  friend class JitUI;
  friend class JitUpdateUI;
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Model)

  W_OBJECT(Model)
public:
  Model(
      TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>&,
      QObject* parent);
  ~Model() override;

  Model(DataStream::Deserializer& vis, QObject* parent);
  Model(JSONObject::Deserializer& vis, QObject* parent);
  Model(DataStream::Deserializer&& vis, QObject* parent);
  Model(JSONObject::Deserializer&& vis, QObject* parent);

  bool validate(const QString& txt) const noexcept;

  const QString& script() const { return m_text; }
  [[nodiscard]] Process::ScriptChangeResult setScript(const QString& txt);
  void scriptChanged(const QString& txt) W_SIGNAL(scriptChanged, txt);
  void programChanged() W_SIGNAL(programChanged);

  static constexpr bool hasExternalUI() noexcept { return true; }

  QString prettyName() const noexcept override;

  std::shared_ptr<NodeFactory> factory;

  void errorMessage(int line, const QString& e) W_SIGNAL(errorMessage, line, e);
  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  std::shared_ptr<NodeFactory> getJitFactory();

  QString effect() const noexcept override;
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;

  void init();
  [[nodiscard]] Process::ScriptChangeResult reload();
  QString m_text;
  std::unique_ptr<NodeCompiler> m_compiler;
};

struct LanguageSpec
{
  static constexpr const char* language = "C++";
};

using JitEffectFactory = Process::EffectProcessFactory_T<AvndJit::Model>;
using LayerFactory = Process::EffectLayerFactory_T<
    Model, Process::ProcessScriptEditDialog<Model, Model::p_script, LanguageSpec>>;
}

namespace Process
{
template <>
QString EffectProcessFactory_T<AvndJit::Model>::customConstructionData() const noexcept;

template <>
Process::Descriptor
EffectProcessFactory_T<AvndJit::Model>::descriptor(QString d) const noexcept;

template <>
Process::Descriptor EffectProcessFactory_T<AvndJit::Model>::descriptor(
    const Process::ProcessModel& d) const noexcept;
}

namespace AvndJit
{
class Executor final
    : public Execution::ProcessComponent_T<AvndJit::Model, ossia::node_process>
{
  COMPONENT_METADATA("086fbfcf-44e9-4ea6-baad-b4f76e938d69")

public:
  static constexpr bool is_unique = true;

  Executor(AvndJit::Model& proc, const Execution::Context& ctx, QObject* parent);
  ~Executor() override;
};
using JitEffectComponentFactory = Execution::ProcessComponentFactory_T<Executor>;
}

namespace AvndJit
{
class EditAvndScript : public Scenario::EditScript<Model, Model::p_script>
{
  SCORE_COMMAND_DECL(Jit::CommandFactoryName(), EditAvndScript, "Edit a C++ program")
public:
  using Scenario::EditScript<Model, Model::p_script>::EditScript;
};

}

namespace score
{
template <>
struct StaticPropertyCommand<AvndJit::Model::p_script> : AvndJit::EditAvndScript
{
  using AvndJit::EditAvndScript::EditAvndScript;
};
}
