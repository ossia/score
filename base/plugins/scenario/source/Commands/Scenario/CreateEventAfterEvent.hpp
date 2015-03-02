#pragma once
#include <tools/SettableIdentifier.hpp>
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>
#include <QMap>
#include <tuple>
#include <ProcessInterface/TimeValue.hpp>

struct EventData;
class EventModel;
class ProcessViewModelInterface;
class AbstractConstraintViewModel;
class ConstraintModel;
class TimeNodeModel;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The CreateEventAfterEventCommand class
        *
        * This Command creates a constraint and another event in a scenario,
        * starting from an event selected by the user.
        *
        * The Command doesn't end on another event / timenode
        */
        class CreateEventAfterEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(CreateEventAfterEvent, "ScenarioControl")
                CreateEventAfterEvent(ObjectPath&& scenarioPath, EventData data);
                CreateEventAfterEvent& operator= (CreateEventAfterEvent &&) = default;

                id_type<TimeNodeModel> createdTimeNode() const
                {
                    return m_createdTimeNodeId;
                }
                id_type<EventModel> createdEvent() const
                {
                    return m_createdEventId;
                }

                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith(const QUndoCommand* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                id_type<ConstraintModel> m_createdConstraintId {};
                id_type<EventModel> m_createdEventId {};
                id_type<TimeNodeModel> m_createdTimeNodeId {};

                id_type<EventModel> m_firstEventId {};
                TimeValue m_time {};
                double m_heightPosition {};

                QMap<std::tuple<int, int, int>, id_type<AbstractConstraintViewModel>> m_createdConstraintViewModelIDs;
                id_type<AbstractConstraintViewModel> m_createdConstraintFullViewId {};
        };
    }
}
