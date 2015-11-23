#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <QMap>
#include <tuple>
class ConstraintViewModel;
class RackModel;
class ConstraintModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The ClearConstraint class
         *
         * Removes all the processes and the rackes of a constraint.
         */
        class ClearConstraint final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ClearConstraint, "Clear a constraint")
            public:
                ClearConstraint(Path<ConstraintModel>&& constraintPath);
                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<ConstraintModel> m_path;

                QVector<QByteArray> m_serializedRackes;
                QVector<QByteArray> m_serializedProcesses;

                QMap<Id<ConstraintViewModel>, Id<RackModel>> m_rackMappings;
        };
    }
}
