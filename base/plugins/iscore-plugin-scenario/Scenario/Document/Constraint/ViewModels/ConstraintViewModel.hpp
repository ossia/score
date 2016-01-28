#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <QString>
#include <nano_signal_slot.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>

class QObject;
namespace Scenario
{
class RackModel;
class ConstraintModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintViewModel :
        public IdentifiedObject<ConstraintViewModel>,
        public Nano::Observer
{
        Q_OBJECT

    public:
        ConstraintViewModel(const Id<ConstraintViewModel>& id,
                            const QString& name,
                            const ConstraintModel& model,
                            QObject* parent);

        virtual ~ConstraintViewModel();

        template<typename DeserializerVisitor>
        ConstraintViewModel(DeserializerVisitor&& vis,
                            const ConstraintModel& model,
                            QObject* parent) :
            IdentifiedObject{vis, parent},
            m_model {model}
        {
            vis.writeTo(*this);
        }

        virtual ConstraintViewModel* clone(
                const Id<ConstraintViewModel>& id,
                const ConstraintModel& cm,
                QObject* parent) = 0;

        virtual QString type() const = 0;
        const ConstraintModel& model() const;

        bool isRackShown() const;
        const Id<RackModel>& shownRack() const;

        void hideRack();
        void showRack(const Id<RackModel>& rackId);

        virtual void on_rackRemoval(const RackModel&);

    signals:
        void lastRackRemoved();
        void rackHidden();
        void rackShown(const Id<RackModel>&);

        void aboutToBeDeleted(ConstraintViewModel*);

    private:
        // A view model cannot be constructed without a model
        // hence we are safe with a pointer
        const ConstraintModel& m_model;

        Id<RackModel> m_shownRack {};
};
}
