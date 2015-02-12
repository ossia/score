#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

#include <core/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
    template<class T>
        class ChangeElementComments : public iscore::SerializableCommand
        {

            public:
            ChangeElementComments(ObjectPath&& path, QString newComments) :
                SerializableCommand{"ScenarioControl",
                                    "Change Comments",
                                    QObject::tr("Change current objects comments")},
                m_newComments{newComments},
                m_path{std::move(path)}
            {

            }

                virtual void undo() override
            {
                auto obj = m_path.find<T>();
                obj->metadata.setComment(m_oldComments);
            }
                virtual void redo() override
            {
                auto obj = m_path.find<T>();
                obj->metadata.setComment(m_newComments);
            }
                virtual int id() const override
            {
                return 1;
            }

                virtual bool mergeWith(const QUndoCommand* other) override
            {
                return false;
            }

            protected:
                virtual void serializeImpl(QDataStream& s) const override
            {
                s << m_path << m_oldComments << m_newComments;
            }

                virtual void deserializeImpl(QDataStream& s) override
            {
                s >> m_path >> m_oldComments >> m_newComments;
            }

            private:
                ObjectPath m_path{};
                QString m_oldComments{};
                QString m_newComments;
        };
    }
}
