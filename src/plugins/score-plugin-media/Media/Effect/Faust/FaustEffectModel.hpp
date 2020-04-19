#pragma once
#if defined(HAS_FAUST)
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Process/Process.hpp>

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
  FaustEffectModel(
      TimeVal t,
      const QString& faustProgram,
      const Id<Process::ProcessModel>&,
      QObject* parent);
  ~FaustEffectModel();

  template <typename Impl>
  FaustEffectModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  const QString& text() const { return m_text; }

  QString prettyName() const noexcept override;
  void setText(const QString& txt);

  Process::Inlets& inlets() { return m_inlets; }
  Process::Outlets& outlets() { return m_outlets; }

  bool hasExternalUI() const noexcept { return false; }

  llvm_dsp_factory* faust_factory{};
  llvm_dsp* faust_object{};

  dsp_poly_factory* faust_poly_factory{};
  dsp_poly* faust_poly_object{};

  void changed()
  W_SIGNAL(changed);

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
QString EffectProcessFactory_T<
    Media::Faust::FaustEffectModel>::customConstructionData() const;

template <>
Process::Descriptor
EffectProcessFactory_T<Media::Faust::FaustEffectModel>::descriptor(
    QString d) const;
}
class QPlainTextEdit;
namespace Media::Faust
{
struct FaustEditDialog : public QDialog
{
  const FaustEffectModel& m_effect;

  QPlainTextEdit* m_textedit{};

public:
  FaustEditDialog(
      const FaustEffectModel& e,
      const score::DocumentContext& ctx,
      QWidget* parent);

  QString text() const;
};

class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Media::Faust::FaustEffectModel>
{
public:
  explicit InspectorWidget(
      const Media::Faust::FaustEffectModel& object,
      const score::DocumentContext& doc,
      QWidget* parent);

private:
  QPlainTextEdit* m_textedit{};
};

class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<FaustEffectModel, InspectorWidget>
{
  SCORE_CONCRETE("6f1b2f7f-29ec-4ba4-b07e-8aa227ec3806")
};

using FaustEffectFactory = Process::EffectProcessFactory_T<FaustEffectModel>;
using LayerFactory = Process::EffectLayerFactory_T<
    FaustEffectModel,
    Process::DefaultEffectItem,
    FaustEditDialog>;
}

namespace Execution
{
class FaustEffectComponent final : public Execution::ProcessComponent_T<
                                       Media::Faust::FaustEffectModel,
                                       ossia::node_process>
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
  template<typename T>
  void reload();
};
using FaustEffectComponentFactory
    = Execution::ProcessComponentFactory_T<FaustEffectComponent>;
}
#endif
