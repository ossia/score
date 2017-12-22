#pragma once
#include <Process/Dataflow/Port.hpp>
#include <score/model/Entity.hpp>

#include <score/model/Component.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score_lib_process_export.h>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
class QGraphicsItem;
namespace Process
{
class Port;
}
namespace Control
{
struct RectItem;
using EffectItem = RectItem;
}
namespace Process
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
class SCORE_LIB_PROCESS_EXPORT EffectModel :
        public score::Entity<EffectModel>,
        public score::SerializableInterface<EffectModel>
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
    public:
        EffectModel(
                const Id<EffectModel>&,
                QObject* parent);

        template<typename Impl>
        EffectModel(
                Impl& vis,
                QObject* parent):
            Entity{vis, parent}
        {
          vis.writeTo(*this);
        }

        virtual QString prettyName() const = 0;

        virtual ~EffectModel();

        const Process::Inlets& inlets() const { return m_inlets; }
        const Process::Outlets& outlets() const { return m_outlets; }

        virtual void showUI();
        virtual void hideUI();

        Process::Inlet* inlet(const Id<Process::Port>&) const;
        Process::Outlet* outlet(const Id<Process::Port>&) const;
  signals:
    void controlAdded(const Id<Process::Port>&);
    void controlRemoved(const Id<Process::Port>&);

  protected:
        Process::Inlets m_inlets;
        Process::Outlets m_outlets;
};
}

#define EFFECT_METADATA(Export, Model, Uuid, ObjectKey, PrettyName, Category, Tags) \
    MODEL_METADATA(Export, Process::EffectModel, Model, Uuid, ObjectKey, PrettyName) \
  CATEGORY_METADATA(Export, Model, Category)                         \
  TAGS_METADATA(Export, Model, Tags)

Q_DECLARE_METATYPE(Id<Process::EffectModel>)

TR_TEXT_METADATA(, Process::EffectModel, PrettyName_k, "Effect")
