#pragma once
#include <Process/Process.hpp>
#include <Effect/EffectFactory.hpp>
#include <Media/Effect/DefaultEffectItem.hpp>
#include <Media/Effect/EffectExecutor.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <QDialog>

#include <faust/dsp/llvm-c-dsp.h>
namespace Media::Faust
{
class FaustEffectModel;
}

PROCESS_METADATA(, Media::Faust::FaustEffectModel, "5354c61a-1649-4f59-b952-5c2f1b79c1bd", "Faust", "Faust", "Audio", {}, Process::ProcessFlags::ExternalEffect)
DESCRIPTION_METADATA(, Media::Faust::FaustEffectModel, "Faust")
namespace Media::Faust
{
class FaustEffectModel :
    public Process::ProcessModel
{
    friend class FaustUI;
    friend class FaustUpdateUI;
    Q_OBJECT
    SCORE_SERIALIZE_FRIENDS
    PROCESS_METADATA_IMPL(FaustEffectModel)

    public:
      FaustEffectModel(
        TimeVal t,
        const QString& faustProgram,
        const Id<Process::ProcessModel>&,
        QObject* parent);
    ~FaustEffectModel();

    template<typename Impl>
    FaustEffectModel(
        Impl& vis,
        QObject* parent) :
      Process::ProcessModel{vis, parent}
    {
      vis.writeTo(*this);
      init();
    }

    const QString& text() const
    {
      return m_text;
    }

    QString prettyName() const override;
    void setText(const QString& txt);

    Process::Inlets& inlets() { return m_inlets; }
    Process::Outlets& outlets() { return m_outlets; }

    llvm_dsp_factory* faust_factory{};
    llvm_dsp* faust_object{};
  private:
    void init();
    void reload();
    QString m_text;
};

}

namespace Process
{
template<>
inline QString EffectProcessFactory_T<Media::Faust::FaustEffectModel>::customConstructionData() const
{
  return "process = _;";
}
}
class QPlainTextEdit;
namespace Media::Faust
{
struct FaustEditDialog : public QDialog
{
    const FaustEffectModel& m_effect;

    QPlainTextEdit* m_textedit{};
  public:
    FaustEditDialog(const FaustEffectModel& e, const score::DocumentContext& ctx, QWidget* parent);

    QString text() const;
};

using FaustEffectFactory = Process::EffectProcessFactory_T<FaustEffectModel>;
using LayerFactory = Process::EffectLayerFactory_T<FaustEffectModel, Media::Effect::DefaultEffectItem, FaustEditDialog>;

}


namespace Engine::Execution
{
class FaustEffectComponent final
    : public Engine::Execution::ProcessComponent_T<Media::Faust::FaustEffectModel, ossia::node_process>
{
    Q_OBJECT
    COMPONENT_METADATA("eb4f83af-5ddc-4f2f-9426-6f8a599a1e96")

    public:
      static constexpr bool is_unique = true;

    FaustEffectComponent(
        Media::Faust::FaustEffectModel& proc,
        const Engine::Execution::Context& ctx,
        const Id<score::Component>& id,
        QObject* parent);
};
using FaustEffectComponentFactory = Engine::Execution::ProcessComponentFactory_T<FaustEffectComponent>;
}
