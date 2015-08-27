#pragma once
#include "Document/Constraint/ViewModels/ConstraintViewModelSerialization.hpp"
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
        class RemoveSelection : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("RemoveSelection", "RemoveSelection")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveSelection, "ScenarioControl")
                RemoveSelection(ModelPath<ScenarioModel>&& scenarioPath, Selection sel);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<ScenarioModel> m_path;

                // For timenodes that may be removed when there is only a single event
                QVector<QPair<id_type<TimeNodeModel>, QByteArray>> m_maybeRemovedTimeNodes;

                QVector<QPair<id_type<StateModel>, QByteArray>> m_removedStates;
                QVector<QPair<id_type<EventModel>, QByteArray>> m_removedEvents;
                QVector<QPair<id_type<TimeNodeModel>, QByteArray>> m_removedTimeNodes;
                QVector<
                    QPair<
                        QPair<
                            id_type<ConstraintModel>,
                            QByteArray
                        >, SerializedConstraintViewModels
                    >
                > m_removedConstraints; // The handlers inside the events are IN the constraints / Rackes / etc.

        };
    }
}
