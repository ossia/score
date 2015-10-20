#pragma once
#include "Slot/SlotModel.hpp"
#include "ProcessInterface/ModelMetadata.hpp"

#include <ProcessInterface/TimeValue.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class ConstraintModel;
class Process;

/**
 * @brief The RackModel class
 *
 * A Rack is a slot container.
 * A Rack is always found in a Constraint.
 */
class RackModel : public IdentifiedObject<RackModel>
{
        Q_OBJECT
        ISCORE_METADATA("RackModel") // TODO use this everywhere.

    public:
        RackModel(const Id<RackModel>& id, QObject* parent);

        // Copy
        RackModel(const RackModel& source,
                 const Id<RackModel>& id,
                 std::function<void(const SlotModel&, SlotModel&)> lmCopyMethod,
                 QObject* parent);

        template<typename Impl>
        RackModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            initConnections();
            vis.writeTo(*this);
        }

        // A rack is necessarily child of a constraint.
        ConstraintModel& constraint() const;

        void addSlot(SlotModel* m, int position);
        void addSlot(SlotModel* m);  // No position : at the end

        void swapSlots(const Id<SlotModel>& firstslot,
                       const Id<SlotModel>& secondslot);

        int slotPosition(const Id<SlotModel>& slotId) const
        {
            return m_positions.indexOf(slotId);
        }

        const QList<Id<SlotModel>>& slotsPositions() const
        { return m_positions; }

        NotifyingMap<SlotModel> slotmodels;
    signals:
        void slotPositionsChanged();

        void on_deleteSharedProcessModel(const Process&);
        void on_durationChanged(const TimeValue&);

    private:
        void initConnections();
        void on_slotRemoved(const SlotModel&);

        // Positions of the slots. First is topmost.
        QList<Id<SlotModel>> m_positions;
};

