#pragma once
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ObjectPath.hpp>
#include <QMap>
#include <tuple>
class AbstractConstraintViewModel;
class BoxModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The ClearConstraint class
         *
         * Removes all the processes and the boxes of a constraint.
         */
        class ClearConstraint : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ClearConstraint, "ScenarioControl")
                ClearConstraint(ObjectPath&& constraintPath);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                QVector<QByteArray> m_serializedBoxes;
                QVector<QByteArray> m_serializedProcesses;

                QMap<id_type<AbstractConstraintViewModel>, id_type<BoxModel>> m_boxMappings;
        };
    }
}
