#pragma once
#include "Slot/SlotModel.hpp"
#include "source/Document/ModelMetadata.hpp"

#include <ProcessInterface/TimeValue.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
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
            vis.writeTo(*this);
        }

        // A rack is necessarily child of a constraint.
        ConstraintModel& constraint() const;

        void addSlot(SlotModel* m, int position);
        void addSlot(SlotModel* m);  // No position : at the end

        void removeSlot(const Id<SlotModel>& slotId);
        void swapSlots(const Id<SlotModel>& firstslot,
                       const Id<SlotModel>& secondslot);

        SlotModel& slot(const Id<SlotModel>& slotId) const;
        int slotPosition(const Id<SlotModel>& slotId) const
        {
            return m_positions.indexOf(slotId);
        }

        const auto& getSlots() const // here we use the 'get' prefix, because 'slots' is keyWord for Qt ...
        { return m_slots; }

        const QList<Id<SlotModel>>& slotsPositions() const
        { return m_positions; }

    signals:
        void slotCreated(const Id<SlotModel>& id);
        void slotRemoved(const Id<SlotModel>& id);
        void slotPositionsChanged();

        void on_deleteSharedProcessModel(const Id<Process>& processId);
        void on_durationChanged(const TimeValue& dur);

    private:
        IdContainer<SlotModel> m_slots;

        // Positions of the slots. First is topmost.
        QList<Id<SlotModel>> m_positions;
};

