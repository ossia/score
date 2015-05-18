#pragma once
#include "MoveEvent.hpp"

namespace Scenario
{
namespace Command
{
class MoveEventAndConstraint : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("MoveEventAndConstraint", "MoveEventAndConstraint")

    public:
        MoveEventAndConstraint();
        MoveEventAndConstraint(
                ObjectPath&& scenarioPath,
                id_type<ConstraintModel> constraintId,
                id_type<EventModel> eventId,
                const TimeValue& date,
                double height,
                ExpandMode mode);

        void undo() override;
        void redo() override;

        void update(
                const ObjectPath& scenarioPath,
                const id_type<ConstraintModel>& constraintId,
                const id_type<EventModel>& eventId,
                const TimeValue& date,
                double height,
                ExpandMode mode)
        {
            m_cmd->update(scenarioPath, eventId, date, height, mode);
            m_constraintHeight = height;
        }

    protected:
        void serializeImpl(QDataStream& s) const override;
        void deserializeImpl(QDataStream& s) override;

    private:
        MoveEvent* m_cmd{};
        id_type<ConstraintModel> m_constraintId;
        double m_constraintHeight{};
};
}
}
