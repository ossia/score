#pragma once
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Process/Process.hpp>
#include <Process/Script/ScriptEditor.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <QDialog>

#include <iostream> // needed by poly-llvm-dsp.h...
#include <verdigris>

#include <faust/dsp/poly-llvm-dsp.h>
namespace Faust
{
class FaustEffectModel;
}

namespace ossia::nodes
{
struct custom_dsp_poly_factory;
class custom_dsp_poly_effect;
}

PROCESS_METADATA(
    , Faust::FaustEffectModel, "5354c61a-1649-4f59-b952-5c2f1b79c1bd", "Faust", "Faust",
    Process::ProcessCategory::Script, "Plugins",
    "Faust process. Refer to https://faust.grame.fr", "GRAME and the Faust team",
    {"Script"}, {}, {}, QUrl("https://ossia.io/score-docs/processes/faust.html"),
    Process::ProcessFlags::ExternalEffect | Process::ProcessFlags::DynamicPorts)
DESCRIPTION_METADATA(, Faust::FaustEffectModel, "Faust")
namespace Faust
{
class FaustEffectModel : public Process::ProcessModel
{
  friend class FaustUI;
  friend class FaustUpdateUI;
  W_OBJECT(FaustEffectModel)
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(FaustEffectModel)

public:
  static constexpr bool hasExternalUI() noexcept { return true; }

  FaustEffectModel(
      TimeVal t, const QString& faustProgram, const Id<Process::ProcessModel>&,
      QObject* parent);
  ~FaustEffectModel();

  template <typename Impl>
  FaustEffectModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  QString prettyName() const noexcept override;

  bool validate(const QString& txt) const noexcept;
  const QString& script() const { return m_script; }
  [[nodiscard]] Process::ScriptChangeResult setScript(const QString& txt);

  Process::Inlets& inlets() noexcept { return m_inlets; }
  Process::Outlets& outlets() noexcept { return m_outlets; }
  const Process::Inlets& inlets() const noexcept { return m_inlets; }
  const Process::Outlets& outlets() const noexcept { return m_outlets; }

  std::shared_ptr<llvm_dsp_factory> faust_factory{};
  std::shared_ptr<llvm_dsp> faust_object{};

  std::shared_ptr<ossia::nodes::custom_dsp_poly_factory> faust_poly_factory{};
  std::shared_ptr<ossia::nodes::custom_dsp_poly_effect> faust_poly_object{};

  void scriptChanged(const QString& str) W_SIGNAL(scriptChanged, str);
  void programChanged() W_SIGNAL(programChanged);

  void errorMessage(int line, const QString& e) W_SIGNAL(errorMessage, line, e);

  PROPERTY(QString, script READ script WRITE setScript NOTIFY scriptChanged)
private:
  QString effect() const noexcept override;
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;

  void init();
  [[nodiscard]] Process::ScriptChangeResult reload();

  QString m_script;
  QString m_path;
  QString m_declareName;
};
}

namespace Process
{
template <>
QString
EffectProcessFactory_T<Faust::FaustEffectModel>::customConstructionData() const noexcept;

template <>
Process::Descriptor
EffectProcessFactory_T<Faust::FaustEffectModel>::descriptor(QString d) const noexcept;
}

namespace Faust
{
struct LanguageSpec
{
  static constexpr const char* language = "Faust";
};

using FaustEffectFactory = Process::EffectProcessFactory_T<FaustEffectModel>;
using LayerFactory = Process::EffectLayerFactory_T<
    FaustEffectModel, Process::ProcessScriptEditDialog<
                          FaustEffectModel, FaustEffectModel::p_script, LanguageSpec>>;
}

namespace Execution
{
class FaustEffectComponent final
    : public Execution::ProcessComponent_T<Faust::FaustEffectModel, ossia::node_process>
{
  W_OBJECT(FaustEffectComponent)
  COMPONENT_METADATA("eb4f83af-5ddc-4f2f-9426-6f8a599a1e96")

public:
  static constexpr bool is_unique = true;

  FaustEffectComponent(
      Faust::FaustEffectModel& proc, const Execution::Context& ctx, QObject* parent);

private:
  int generation{};
  void reload(Execution::Transaction&);
  void reloadSynth(Execution::Transaction&);
  void reloadFx(Execution::Transaction&);

  template <typename Node_T>
  void setupExecutionControls(const Node_T&, int firstControlIndex);
  template <typename Node_T>
  void setupExecutionControlOutlets(const Node_T&, int firstControlIndex);

  std::vector<QMetaObject::Connection> m_controlConnections;
};
using FaustEffectComponentFactory
    = Execution::ProcessComponentFactory_T<FaustEffectComponent>;
}
