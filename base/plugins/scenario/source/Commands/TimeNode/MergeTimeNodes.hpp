#pragma once

#include <core/presenter/command/SerializableCommand.hpp>

#include <core/tools/ObjectPath.hpp>


class EventModel;
class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
        class MergeTimeNodes : public iscore::SerializableCommand
        {
            public:
                MergeTimeNodes(ObjectPath&& path, id_type<TimeNodeModel> aimedTimeNode, id_type<TimeNodeModel> movingTimeNode);
                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith(const QUndoCommand* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                id_type<TimeNodeModel> m_aimedTimeNodeId;
                id_type<TimeNodeModel> m_movingTimeNodeId;

                QByteArray m_serializedTimeNode;
         };
    }
}
