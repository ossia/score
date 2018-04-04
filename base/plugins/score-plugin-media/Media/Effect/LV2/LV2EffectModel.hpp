#pragma once
#include <Process/Process.hpp>
#include <Effect/EffectFactory.hpp>
#include <lilv/lilvmm.hpp>
#include <Media/Effect/LV2/LV2Context.hpp>
#include <QJsonDocument>
#include <Media/Effect/DefaultEffectItem.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <QInputDialog>

namespace Media::LV2
{
class LV2EffectModel;
}
PROCESS_METADATA(, Media::LV2::LV2EffectModel, "fd5243ba-70b5-4164-b44a-ecb0dcdc0494", "LV2", "LV2", "Audio", {}, Process::ProcessFlags::ExternalEffect)
DESCRIPTION_METADATA(, Media::LV2::LV2EffectModel, "LV2")
namespace Media::LV2
{
/** LV2 effect model.
 * Should contain an effect, maybe instantiated with
 * LibMediaStream's MakeLV2MediaEffect
 * Cloning can be done with MakeCopyEffect.
 */
class LV2EffectModel :
        public Process::ProcessModel
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
    public:
        PROCESS_METADATA_IMPL(LV2EffectModel)
        LV2EffectModel(
                TimeVal t,
                const QString& name,
                const Id<Process::ProcessModel>&,
                QObject* parent);

        template<typename Impl>
        LV2EffectModel(
                Impl& vis,
                QObject* parent) :
            ProcessModel{vis, parent}
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
}

namespace Process
{
template<>
QString EffectProcessFactory_T<Media::LV2::LV2EffectModel>::customConstructionData() const;
}

namespace Media::LV2
{
using LV2EffectFactory = Process::EffectProcessFactory_T<LV2EffectModel>;
using LayerFactory = Process::EffectLayerFactory_T<LV2EffectModel, Media::Effect::DefaultEffectItem>;
}

namespace Engine
{
namespace Execution
{
class LV2EffectComponent final
    : public Engine::Execution::ProcessComponent_T<Media::LV2::LV2EffectModel, ossia::node_process>
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
using LV2EffectComponentFactory = Engine::Execution::ProcessComponentFactory_T<LV2EffectComponent>;
}
}
