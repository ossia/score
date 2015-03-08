#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/SerializableCommand.hpp>
class AbstractConstraintViewModel;

#include <iscore/tools/ObjectPath.hpp>

class ConstraintModel;
class EventModel;

namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The RemoveConstraint class
        *
        * remove an event
        *
        */
        class RemoveConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(RemoveConstraint, "ScenarioControl")
                RemoveConstraint(ObjectPath&& scenarioPath, ConstraintModel* constraint);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<ConstraintModel> m_cstrId {};
                QByteArray m_serializedConstraint;

                id_type<EventModel> m_startEvent;
                id_type<EventModel> m_endEvent;

                QMap<std::tuple<int, int, int>, id_type<AbstractConstraintViewModel>> m_constraintViewModelIDs;
                id_type<AbstractConstraintViewModel> m_constraintFullViewId {};
        };
    }
}
