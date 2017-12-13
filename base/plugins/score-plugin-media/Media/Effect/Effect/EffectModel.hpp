#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Media/Effect/Effect/EffectFactory.hpp>
#include <score/model/Entity.hpp>

#include <Engine/Executor/ExecutorContext.hpp>
#include <score/model/Component.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score_plugin_media_export.h>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
class QGraphicsItem;
namespace Process
{
class Port;
}
namespace Media
{
namespace Effect
{
/**
 * @brief The EffectModel class
 *
 * Abstract class for an effect instance.
 * A concrete example class is FaustEffectModel,
 * which represents a Faust effect; each instance
 * of FaustEffectModel could be a different effect (e.g.
 * reverb, distorsion, etc.)
 */
class SCORE_PLUGIN_MEDIA_EXPORT EffectModel :
        public score::Entity<EffectModel>,
        public score::SerializableInterface<EffectFactory>
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
    public:
        EffectModel(
                const Id<EffectModel>&,
                QObject* parent);

        EffectModel(
                const EffectModel& source,
                const Id<EffectModel>&,
                QObject* parent);

        template<typename Impl>
        EffectModel(
                Impl& vis,
                QObject* parent) :
            Entity{vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual QString prettyName() const = 0;

        virtual ~EffectModel();

        virtual EffectModel* clone(
                const Id<EffectModel>& newId,
                QObject* parent) const = 0;

        const Process::Inlets& inlets() const { return m_inlets; }
        const Process::Outlets& outlets() const { return m_outlets; }

        virtual QGraphicsItem* makeItem(const score::DocumentContext& ctx);
        virtual std::shared_ptr<ossia::audio_fx_node> makeNode(const Engine::Execution::Context &, QObject* ctx);
        virtual void showUI();
        virtual void hideUI();

    signals:
        void effectChanged() const;

  protected:
        Process::Inlets m_inlets;
        Process::Outlets m_outlets;
};
}
}

#define EFFECT_METADATA(Export, Model, Uuid, ObjectKey, PrettyName, Category, Tags) \
    MODEL_METADATA(Export, Media::Effect::EffectFactory, Model, Uuid, ObjectKey, PrettyName) \
  CATEGORY_METADATA(Export, Model, Category)                         \
  TAGS_METADATA(Export, Model, Tags)

Q_DECLARE_METATYPE(Id<Media::Effect::EffectModel>)

TR_TEXT_METADATA(, Media::Effect::EffectModel, PrettyName_k, "Effect")
