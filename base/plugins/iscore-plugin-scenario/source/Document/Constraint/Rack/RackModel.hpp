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
        ModelMetadata metadata; // TODO REMOVEME

        RackModel(const id_type<RackModel>& id, QObject* parent);

        // Copy
        RackModel(const RackModel& source,
                 const id_type<RackModel>& id,
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

        void removeSlot(const id_type<SlotModel>& slotId);
        void swapSlots(const id_type<SlotModel>& firstslot,
                       const id_type<SlotModel>& secondslot);

        SlotModel& slot(const id_type<SlotModel>& slotId) const;
        int slotPosition(const id_type<SlotModel>& slotId) const
        {
            return m_positions.indexOf(slotId);
        }

        const auto& getSlots() const // here we use the 'get' prefix, because 'slots' is keyWord for Qt ...
        { return m_slots; }

        const QList<id_type<SlotModel>>& slotsPositions() const
        { return m_positions; }

    signals:
        void slotCreated(const id_type<SlotModel>& id);
        void slotRemoved(const id_type<SlotModel>& id);
        void slotPositionsChanged();

        void on_deleteSharedProcessModel(const id_type<Process>& processId);
        void on_durationChanged(const TimeValue& dur);

    private:
        IdContainer<SlotModel> m_slots;

        // Positions of the slots. First is topmost.
        QList<id_type<SlotModel>> m_positions;
};

