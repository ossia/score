#pragma once
#if __has_include(<score_plugin_gfx.hpp>)
#define SCORE_JIT_HAS_TEXGEN 1
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/Script/ScriptEditor.hpp>
#include <Process/Script/ScriptProcess.hpp>

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
class TexgenModel;
}
PROCESS_METADATA(
    , Jit::TexgenModel, "b9a20181-2925-4ade-925e-a2fd05fcbf9b", "Jit",
    "C++ texture generator", Process::ProcessCategory::Script, "Visuals/Textures",
    "Generate a texture", "ossia score", QStringList{}, {}, {},
    QUrl("https://ossia.io/score-docs/processes/texgen.html#texture-generator"),
    Process::ProcessFlags::SupportsAll)
namespace Jit
{
struct Driver;
using TexgenFunction = void(unsigned char* rgb, int width, int height, int t);
using TexgenCompiler = Driver;
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
      TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>&,
      QObject* parent);
  ~TexgenModel() override;

  TexgenModel(DataStream::Deserializer& vis, QObject* parent);
  TexgenModel(JSONObject::Deserializer& vis, QObject* parent);
  TexgenModel(DataStream::Deserializer&& vis, QObject* parent);
  TexgenModel(JSONObject::Deserializer&& vis, QObject* parent);

  bool validate(const QString& txt) const noexcept;
  const QString& script() const noexcept { return m_text; }
  [[nodiscard]] Process::ScriptChangeResult setScript(const QString& txt);
  void scriptChanged(const QString& txt) W_SIGNAL(scriptChanged, txt);
  void programChanged() W_SIGNAL(programChanged);

  static constexpr bool hasExternalUI() noexcept { return true; }

  QString prettyName() const noexcept override;

  TexgenFactory factory;

  void errorMessage(int line, const QString& e) W_SIGNAL(errorMessage, line, e);

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  QString effect() const noexcept override;
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  void init();
  [[nodiscard]] Process::ScriptChangeResult reload();
  QString m_text;
  std::unique_ptr<TexgenCompiler> m_compiler;
};
}

namespace Process
{
template <>
QString
EffectProcessFactory_T<Jit::TexgenModel>::customConstructionData() const noexcept;

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::TexgenModel>::descriptor(QString d) const noexcept;
template <>
Process::Descriptor EffectProcessFactory_T<Jit::TexgenModel>::descriptor(
    const Process::ProcessModel& d) const noexcept;
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
    TexgenModel, Process::ProcessScriptEditDialog<
                     TexgenModel, TexgenModel::p_script, TexgenLanguageSpec>>;

class TexgenExecutor final
    : public Execution::ProcessComponent_T<Jit::TexgenModel, ossia::node_process>
{
  COMPONENT_METADATA("ec4bd3af-8d81-4d92-9b53-86a34d8108f8")

public:
  static constexpr bool is_unique = true;

  TexgenExecutor(Jit::TexgenModel& proc, const Execution::Context& ctx, QObject* parent);
  ~TexgenExecutor() override;
};
using TexgenExecutorFactory = Execution::ProcessComponentFactory_T<TexgenExecutor>;
}

namespace Jit
{
class EditTexgen : public Scenario::EditScript<TexgenModel, TexgenModel::p_script>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditTexgen, "Edit a texture script")
public:
  using Scenario::EditScript<TexgenModel, TexgenModel::p_script>::EditScript;
};

}

namespace score
{
template <>
struct StaticPropertyCommand<Jit::TexgenModel::p_script> : Jit::EditTexgen
{
  using Jit::EditTexgen::EditTexgen;
};
}

#endif
