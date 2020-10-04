#pragma once
#if __has_include(<score_plugin_gfx.hpp>)
#define SCORE_JIT_HAS_TEXGEN 1
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
class TexgenModel;
}
PROCESS_METADATA(
    ,
    Jit::TexgenModel,
    "b9a20181-2925-4ade-925e-a2fd05fcbf9b",
    "Jit",
    "Texgen",
    Process::ProcessCategory::Script,
    "GFX",
    "Generate a texture",
    "ossia score",
    QStringList{},
    {},
    {},
    Process::ProcessFlags::SupportsAll)
namespace Jit
{
template <typename Fun_T>
struct Driver;
using TexgenFunction = void(unsigned char* rgb, int width, int height, int t);
using TexgenCompiler = Driver<TexgenFunction>;
using TexgenFactory = std::function<TexgenFunction>;
class TexgenModel : public Process::ProcessModel
{
  friend class JitUI;
  friend class JitUpdateUI;
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(TexgenModel)

  W_OBJECT(TexgenModel)
public:
  TexgenModel(
      TimeVal t,
      const QString& jitProgram,
      const Id<Process::ProcessModel>&,
      QObject* parent);
  ~TexgenModel() override;

  TexgenModel(DataStream::Deserializer& vis, QObject* parent);
  TexgenModel(JSONObject::Deserializer& vis, QObject* parent);
  TexgenModel(DataStream::Deserializer&& vis, QObject* parent);
  TexgenModel(JSONObject::Deserializer&& vis, QObject* parent);

  bool validate(const QString& txt) const noexcept;
  const QString& script() const noexcept { return m_text; }
  void setScript(const QString& txt);
  void scriptChanged(const QString& txt) W_SIGNAL(scriptChanged, txt);

  static constexpr bool hasExternalUI() noexcept { return true; }

  QString prettyName() const noexcept override;
  void changed() W_SIGNAL(changed);

  Process::Inlets& inlets() { return m_inlets; }
  Process::Outlets& outlets() { return m_outlets; }

  TexgenFactory factory;

  void errorMessage(int line, const QString& e) W_SIGNAL(errorMessage, line, e);

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  void init();
  void reload();
  QString m_text;
  std::unique_ptr<TexgenCompiler> m_compiler;
};
}

namespace Process
{
template <>
QString
EffectProcessFactory_T<Jit::TexgenModel>::customConstructionData() const;

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::TexgenModel>::descriptor(QString d) const;
}
class QPlainTextEdit;
namespace Jit
{

struct TexgenLanguageSpec
{
  static constexpr const char* language = "C++";
};

using TexgenEffectFactory = Process::EffectProcessFactory_T<TexgenModel>;
using TexgenLayerFactory = Process::EffectLayerFactory_T<
    TexgenModel,
    Process::DefaultEffectItem,
    Process::ProcessScriptEditDialog<TexgenModel, TexgenModel::p_script, TexgenLanguageSpec>
>;

class TexgenExecutor final
    : public Execution::
          ProcessComponent_T<Jit::TexgenModel, ossia::node_process>
{
  COMPONENT_METADATA("ec4bd3af-8d81-4d92-9b53-86a34d8108f8")

public:
  static constexpr bool is_unique = true;

  TexgenExecutor(
      Jit::TexgenModel& proc,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
  ~TexgenExecutor() override;
};
using TexgenExecutorFactory
    = Execution::ProcessComponentFactory_T<TexgenExecutor>;
}

PROPERTY_COMMAND_T(Jit, EditTexgen, TexgenModel::p_script, "Edit texgen")
SCORE_COMMAND_DECL_T(Jit::EditTexgen)

#endif
