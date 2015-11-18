#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelSerialization.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

class EventModel;

namespace Scenario
{
    namespace Command
    {
        /**
        * @brief The RemoveSelection class
        *
        * Tries to remove what is selected in a scenario.
        */
        class RemoveSelection final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), RemoveSelection, "RemoveSelection")
            public:
                RemoveSelection(Path<ScenarioModel>&& scenarioPath, Selection sel);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                Path<ScenarioModel> m_path;

                // For timenodes that may be removed when there is only a single event
                QVector<QPair<Id<TimeNodeModel>, QByteArray>> m_maybeRemovedTimeNodes;

                QVector<QPair<Id<StateModel>, QByteArray>> m_removedStates;
                QVector<QPair<Id<EventModel>, QByteArray>> m_removedEvents;
                QVector<QPair<Id<TimeNodeModel>, QByteArray>> m_removedTimeNodes;
                QVector<
                    QPair<
                        QPair<
                            Id<ConstraintModel>,
                            QByteArray
                        >, SerializedConstraintViewModels
                    >
                > m_removedConstraints; // The handlers inside the events are IN the constraints / Rackes / etc.

        };
    }
}
