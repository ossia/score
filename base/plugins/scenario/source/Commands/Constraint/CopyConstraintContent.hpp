#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
#include "ProcessInterface/ExpandMode.hpp"

class ProcessModel;
class BoxModel;
namespace Scenario
{
    namespace Command
    {
        class CopyConstraintContent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(CopyConstraintContent, "ScenarioControl")
                CopyConstraintContent(QJsonObject&& sourceConstraint,
                                      ObjectPath&&  targetConstraint,
                                      ExpandMode mode);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                QJsonObject m_source;
                ObjectPath m_target;
                ExpandMode m_mode{ExpandMode::Grow};

                QVector<id_type<BoxModel>> m_boxIds;
                QVector<id_type<ProcessModel>> m_processIds;
        };
    }
}
