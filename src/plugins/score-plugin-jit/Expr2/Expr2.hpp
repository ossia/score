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

#include <verdigris>

namespace Jit
{
class Expr2Model;
}
PROCESS_METADATA(
    , Jit::Expr2Model, "45649a5a-62ca-49a6-8865-be3d7299ad62", "Jit", "Expr2",
    Process::ProcessCategory::Script, "Mappings", "Run Expr2 code", "ossia score",
    QStringList{}, {}, {}, QUrl("https://ossia.io/score-docs/processes/jit.html"),
    Process::ProcessFlags::SupportsAll)
namespace Jit
{
struct Driver;
using Expr2Function = void*();
using Expr2Compiler = Driver;
using Expr2Factory = std::function<Expr2Function>;
class Expr2Model : public Process::ProcessModel
{
  friend class JitUI;
  friend class JitUpdateUI;
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Expr2Model)

  W_OBJECT(Expr2Model)
public:
  Expr2Model(
      TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>&,
      QObject* parent);
  ~Expr2Model() override;

  Expr2Model(DataStream::Deserializer& vis, QObject* parent);
  Expr2Model(JSONObject::Deserializer& vis, QObject* parent);
  Expr2Model(DataStream::Deserializer&& vis, QObject* parent);
  Expr2Model(JSONObject::Deserializer&& vis, QObject* parent);

  const QString& script() const noexcept { return m_text; }
  [[nodiscard]] Process::ScriptChangeResult setScript(const QString& txt);
  void scriptChanged(const QString& txt) W_SIGNAL(scriptChanged, txt);

  static constexpr bool hasExternalUI() noexcept { return true; }

  bool validate(const QString& txt) const noexcept;

  QString prettyName() const noexcept override;
  void programChanged() W_SIGNAL(programChanged);

  Expr2Factory factory;

  void errorMessage(int line, const QString& e) W_SIGNAL(errorMessage, line, e);

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  QString effect() const noexcept override;
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  void init();
  [[nodiscard]] Process::ScriptChangeResult reload();
  QString m_text;
  std::unique_ptr<Expr2Compiler> m_compiler;
};
}

namespace Process
{
template <>
QString
EffectProcessFactory_T<Jit::Expr2Model>::customConstructionData() const noexcept;

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::Expr2Model>::descriptor(QString d) const noexcept;

template <>
Process::Descriptor EffectProcessFactory_T<Jit::Expr2Model>::descriptor(
    const Process::ProcessModel& d) const noexcept;
}
class QPlainTextEdit;
namespace Jit
{

struct Expr2LanguageSpec
{
  static constexpr const char* language = "C++";
};

using Expr2EffectFactory = Process::EffectProcessFactory_T<Expr2Model>;
using Expr2LayerFactory = Process::EffectLayerFactory_T<
    Expr2Model, Process::DefaultEffectItem,
    Process::ProcessScriptEditDialog<
        Expr2Model, Expr2Model::p_script, Expr2LanguageSpec>>;

class Expr2Executor final
    : public Execution::ProcessComponent_T<Jit::Expr2Model, ossia::node_process>
{
  COMPONENT_METADATA("dc4f88ae-ca36-4330-b0e7-8093a1793521")

public:
  static constexpr bool is_unique = true;

  Expr2Executor(
      Jit::Expr2Model& proc, const Execution::Context& ctx, QObject* parent);
  ~Expr2Executor() override;
};
using Expr2ExecutorFactory = Execution::ProcessComponentFactory_T<Expr2Executor>;
}

namespace Jit
{
class EditExpr2 : public Scenario::EditScript<Expr2Model, Expr2Model::p_script>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditExpr2, "Edit a Expr2")
public:
  using Scenario::EditScript<Expr2Model, Expr2Model::p_script>::EditScript;
};

}

namespace score
{
template <>
struct StaticPropertyCommand<Jit::Expr2Model::p_script> : Jit::EditExpr2
{
  using Jit::EditExpr2::EditExpr2;
};
}
