#pragma once
#include <Effect/EffectModel.hpp>
#include <Effect/EffectFactory.hpp>
#include <Media/Effect/EffectExecutor.hpp>

class llvm_dsp_factory;
class llvm_dsp;
namespace Media::Faust
{
class FaustEffectModel;
}

EFFECT_METADATA(, Media::Faust::FaustEffectModel, "5354c61a-1649-4f59-b952-5c2f1b79c1bd", "Faust", "Faust", "", {})

namespace Media::Faust
{
/** Faust effect model.
 * Should contain an effect, maybe instantiated with
 * LibMediaStream's MakeFaustMediaEffect
 * Cloning can be done with MakeCopyEffect.
 */
class FaustEffectModel :
    public Process::EffectModel
{
    friend class FaustUI;
    friend class FaustUpdateUI;
    Q_OBJECT
    SCORE_SERIALIZE_FRIENDS
    MODEL_METADATA_IMPL(FaustEffectModel)

    public:
      FaustEffectModel(
        const QString& faustProgram,
        const Id<EffectModel>&,
        QObject* parent);
    ~FaustEffectModel();

    template<typename Impl>
    FaustEffectModel(
        Impl& vis,
        QObject* parent) :
      EffectModel{vis, parent}
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
    void showUI() override;
    void hideUI() override;

    llvm_dsp_factory* faust_factory{};
    llvm_dsp* faust_object{};
  private:
    void init();
    void reload();
    QString m_text;
};

using FaustEffectFactory = Process::EffectFactory_T<FaustEffectModel>;

}


namespace Engine::Execution
{
class FaustEffectComponent
    : public Engine::Execution::EffectComponent_T<Media::Faust::FaustEffectModel>
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
using FaustEffectComponentFactory = Engine::Execution::EffectComponentFactory_T<FaustEffectComponent>;
}
