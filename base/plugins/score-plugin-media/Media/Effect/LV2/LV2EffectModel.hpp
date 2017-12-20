#pragma once
#include <Media/Effect/Effect/EffectModel.hpp>
#include <lilv/lilvmm.hpp>
#include <Media/Effect/LV2/LV2Context.hpp>
#include <QJsonDocument>
#include <Media/Effect/EffectExecutor.hpp>

namespace Media
{
namespace LV2
{
class LV2EffectModel;
}
}
EFFECT_METADATA(, Media::LV2::LV2EffectModel, "fd5243ba-70b5-4164-b44a-ecb0dcdc0494", "LV2", "LV2", "", {})
namespace Media
{
namespace LV2
{



/** LV2 effect model.
 * Should contain an effect, maybe instantiated with
 * LibMediaStream's MakeLV2MediaEffect
 * Cloning can be done with MakeCopyEffect.
 */
class LV2EffectModel :
        public Effect::EffectModel
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
    public:
        MODEL_METADATA_IMPL(LV2EffectModel)
        LV2EffectModel(
                const QString& name,
                const Id<EffectModel>&,
                QObject* parent);

        template<typename Impl>
        LV2EffectModel(
                Impl& vis,
                QObject* parent) :
            EffectModel{vis, parent}
        {
            vis.writeTo(*this);
            reload();
        }

        const QString& effect() const
        { return m_effectPath; }

        void setEffect(const QString& s)
        { m_effectPath = s; }

        QString prettyName() const override;

        const LilvPlugin* plugin{};
        LV2::EffectContext effectContext;
    private:
        void reload();
        QString m_effectPath;
        void readPlugin();
};
using LV2EffectFactory = Effect::EffectFactory_T<LV2EffectModel>;
}
}

namespace Engine
{
namespace Execution
{
class LV2EffectComponent final
    : public Engine::Execution::EffectComponent_T<Media::LV2::LV2EffectModel>
{
  Q_OBJECT
  COMPONENT_METADATA("57f50003-a179-424a-80b1-b9394d73a84a")

public:
    static constexpr bool is_unique = true;

  LV2EffectComponent(
      Media::LV2::LV2EffectModel& proc,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);
};
using LV2EffectComponentFactory = Engine::Execution::EffectComponentFactory_T<LV2EffectComponent>;
}
}
