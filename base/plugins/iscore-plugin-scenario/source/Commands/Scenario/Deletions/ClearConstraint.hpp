#pragma once
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
        class ClearConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("ClearConstraint", "ClearConstraint")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ClearConstraint, "ScenarioControl")
                ClearConstraint(ModelPath<ConstraintModel>&& constraintPath);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<ConstraintModel> m_path;

                QVector<QByteArray> m_serializedRackes;
                QVector<QByteArray> m_serializedProcesses;

                QMap<id_type<ConstraintViewModel>, id_type<RackModel>> m_rackMappings;
        };
    }
}
