#pragma once
#if defined(HAS_FAUST)
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Process/Process.hpp>
#include <Process/Script/ScriptEditor.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <QDialog>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

#include <verdigris>

#include <faust/dsp/poly-llvm-dsp.h>
namespace Media::Faust
{
class FaustEffectModel;
}

PROCESS_METADATA(
    ,
    Media::Faust::FaustEffectModel,
    "5354c61a-1649-4f59-b952-5c2f1b79c1bd",
    "Faust",
    "Faust",
    Process::ProcessCategory::Script,
    "Audio",
    "Faust process. Refer to https://faust.grame.fr",
    "GRAME and the Faust team",
    {"Script"},
    {},
    {},
    Process::ProcessFlags::ExternalEffect)
DESCRIPTION_METADATA(, Media::Faust::FaustEffectModel, "Faust")
namespace Media::Faust
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
      TimeVal t,
      const QString& faustProgram,
      const Id<Process::ProcessModel>&,
      QObject* parent);
  ~FaustEffectModel();

  template <typename Impl>
  FaustEffectModel(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  const QString& text() const { return m_text; }

  QString prettyName() const noexcept override;
  void setText(const QString& txt);

  Process::Inlets& inlets() noexcept { return m_inlets; }
  Process::Outlets& outlets() noexcept { return m_outlets; }
  const Process::Inlets& inlets() const noexcept { return m_inlets; }
  const Process::Outlets& outlets() const noexcept { return m_outlets; }

  llvm_dsp_factory* faust_factory{};
  llvm_dsp* faust_object{};

  dsp_poly_factory* faust_poly_factory{};
  dsp_poly* faust_poly_object{};

  void changed() W_SIGNAL(changed);
  void textChanged(const QString& str) W_SIGNAL(textChanged, str);

  void errorMessage(int line, const QString& e) W_SIGNAL(errorMessage, line, e);

  PROPERTY(QString, text READ text WRITE setText NOTIFY textChanged)
private:
  void init();
  void reload();
  void reloadFx(llvm_dsp_factory* fac, llvm_dsp* obj);
  void reloadMidi(dsp_poly_factory* fac, dsp_poly* obj);
  QString m_text;
  QString m_declareName;
};
}

namespace Process
{
template <>
QString EffectProcessFactory_T<Media::Faust::FaustEffectModel>::customConstructionData() const;

template <>
Process::Descriptor
EffectProcessFactory_T<Media::Faust::FaustEffectModel>::descriptor(QString d) const;
}

namespace Media::Faust
{
using FaustEffectFactory = Process::EffectProcessFactory_T<FaustEffectModel>;
using LayerFactory = Process::EffectLayerFactory_T<
    FaustEffectModel,
    Process::DefaultEffectItem,
    Process::ProcessScriptEditDialog<FaustEffectModel, FaustEffectModel::p_text>>;
}

namespace Execution
{
class FaustEffectComponent final
    : public Execution::ProcessComponent_T<Media::Faust::FaustEffectModel, ossia::node_process>
{
  W_OBJECT(FaustEffectComponent)
  COMPONENT_METADATA("eb4f83af-5ddc-4f2f-9426-6f8a599a1e96")

public:
  static constexpr bool is_unique = true;

  FaustEffectComponent(
      Media::Faust::FaustEffectModel& proc,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

private:
  void reloadSynth();
  void reloadFx();
  template <typename T>
  void reload();
};
using FaustEffectComponentFactory = Execution::ProcessComponentFactory_T<FaustEffectComponent>;
}
#endif
