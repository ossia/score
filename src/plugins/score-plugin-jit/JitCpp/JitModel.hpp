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
namespace Jit
{
class JitEffectModel;
}

PROCESS_METADATA(
    , Jit::JitEffectModel, "0a3b49d6-4ce7-4668-aec3-9505b6ee1a60", "Jit", "JIT",
    Process::ProcessCategory::Script, "Script", "JIT compilation process", "ossia score",
    QStringList{}, {}, {},
    QUrl("https://ossia.io/score-docs/processes/cpp_jit.html#c-jit"),
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::DynamicPorts)
namespace Jit
{
struct Driver;

using NodeCompiler = Driver;
using NodeFactory = std::function<ossia::graph_node*()>;

class JitEffectModel : public Process::ProcessModel
{
  friend class JitUI;
  friend class JitUpdateUI;
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(JitEffectModel)

  W_OBJECT(JitEffectModel)
public:
  JitEffectModel(
      TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>&,
      QObject* parent);
  ~JitEffectModel() override;

  JitEffectModel(DataStream::Deserializer& vis, QObject* parent);
  JitEffectModel(JSONObject::Deserializer& vis, QObject* parent);
  JitEffectModel(DataStream::Deserializer&& vis, QObject* parent);
  JitEffectModel(JSONObject::Deserializer&& vis, QObject* parent);

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

using JitEffectFactory = Process::EffectProcessFactory_T<Jit::JitEffectModel>;
using LayerFactory = Process::EffectLayerFactory_T<
    JitEffectModel, Process::DefaultEffectItem,
    Process::ProcessScriptEditDialog<
        JitEffectModel, JitEffectModel::p_script, LanguageSpec>>;
}

namespace Process
{
template <>
QString
EffectProcessFactory_T<Jit::JitEffectModel>::customConstructionData() const noexcept;

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::JitEffectModel>::descriptor(QString d) const noexcept;

template <>
Process::Descriptor EffectProcessFactory_T<Jit::JitEffectModel>::descriptor(
    const Process::ProcessModel& d) const noexcept;
}

namespace Execution
{
class JitEffectComponent final
    : public Execution::ProcessComponent_T<Jit::JitEffectModel, ossia::node_process>
{
  COMPONENT_METADATA("122ceaeb-cbcc-4808-91f2-1929e3ca8292")

public:
  static constexpr bool is_unique = true;

  JitEffectComponent(
      Jit::JitEffectModel& proc, const Execution::Context& ctx, QObject* parent);
  ~JitEffectComponent() override;
};
using JitEffectComponentFactory
    = Execution::ProcessComponentFactory_T<JitEffectComponent>;
}

namespace Jit
{
class EditScript : public Scenario::EditScript<JitEffectModel, JitEffectModel::p_script>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditScript, "Edit a C++ program")
public:
  using Scenario::EditScript<JitEffectModel, JitEffectModel::p_script>::EditScript;
};

}

namespace score
{
template <>
struct StaticPropertyCommand<Jit::JitEffectModel::p_script> : Jit::EditScript
{
  using Jit::EditScript::EditScript;
};
}
