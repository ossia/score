#pragma once
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMetadata.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>

#include <Process/Script/ScriptEditor.hpp>
#include <JitCpp/EditScript.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <verdigris>

namespace Jit
{
class BytebeatModel;
}
PROCESS_METADATA(
    ,
    Jit::BytebeatModel,
    "608beeb7-e5c2-40a5-bd1a-aa7aec80f864",
    "Jit",
    "Bytebeat",
    Process::ProcessCategory::Script,
    "Script",
    "Run bytebeat code",
    "ossia score",
    QStringList{},
    {},
    {},
    Process::ProcessFlags::SupportsAll)
namespace Jit
{
template <typename Fun_T>
struct Driver;
using BytebeatFunction = void(double* input, int size, int time);
using BytebeatCompiler = Driver<BytebeatFunction>;
using BytebeatFactory = std::function<BytebeatFunction>;
class BytebeatModel : public Process::ProcessModel
{
  friend class JitUI;
  friend class JitUpdateUI;
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(BytebeatModel)

  W_OBJECT(BytebeatModel)
public:
  BytebeatModel(
      TimeVal t,
      const QString& jitProgram,
      const Id<Process::ProcessModel>&,
      QObject* parent);
  ~BytebeatModel() override;

  BytebeatModel(DataStream::Deserializer& vis, QObject* parent);
  BytebeatModel(JSONObject::Deserializer& vis, QObject* parent);
  BytebeatModel(DataStream::Deserializer&& vis, QObject* parent);
  BytebeatModel(JSONObject::Deserializer&& vis, QObject* parent);

  const QString& script() const noexcept { return m_text; }
  void setScript(const QString& txt);
  void scriptChanged(const QString& txt) W_SIGNAL(scriptChanged, txt);

  static constexpr bool hasExternalUI() noexcept { return true; }

  bool validate(const QString& txt) const noexcept;

  QString prettyName() const noexcept override;
  void changed() W_SIGNAL(changed);

  Process::Inlets& inlets() { return m_inlets; }
  Process::Outlets& outlets() { return m_outlets; }

  BytebeatFactory factory;

  void errorMessage(int line, const QString& e) W_SIGNAL(errorMessage, line, e);

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  void init();
  void reload();
  QString m_text;
  std::unique_ptr<BytebeatCompiler> m_compiler;
};
}

namespace Process
{
template <>
QString
EffectProcessFactory_T<Jit::BytebeatModel>::customConstructionData() const;

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::BytebeatModel>::descriptor(QString d) const;
}
class QPlainTextEdit;
namespace Jit
{

struct BytebeatLanguageSpec
{
  static constexpr const char* language = "C++";
};

using BytebeatEffectFactory = Process::EffectProcessFactory_T<BytebeatModel>;
using BytebeatLayerFactory = Process::EffectLayerFactory_T<
    BytebeatModel,
    Process::DefaultEffectItem,
    Process::ProcessScriptEditDialog<BytebeatModel, BytebeatModel::p_script, BytebeatLanguageSpec>
>;

class BytebeatExecutor final
    : public Execution::
          ProcessComponent_T<Jit::BytebeatModel, ossia::node_process>
{
  COMPONENT_METADATA("dc4f88ae-ca36-4330-b0e7-8093a1793521")

public:
  static constexpr bool is_unique = true;

  BytebeatExecutor(
      Jit::BytebeatModel& proc,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~BytebeatExecutor() override;
};
using BytebeatExecutorFactory
    = Execution::ProcessComponentFactory_T<BytebeatExecutor>;
}

PROPERTY_COMMAND_T(Jit, EditBytebeat, BytebeatModel::p_script, "Edit bytebeat")
SCORE_COMMAND_DECL_T(Jit::EditBytebeat)
