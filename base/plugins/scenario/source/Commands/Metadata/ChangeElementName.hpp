#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

#include <core/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
    template<class T>
        class ChangeElementName : public iscore::SerializableCommand
        {

            public:
            ChangeElementName(ObjectPath&& path, QString newName) :
                SerializableCommand{"ScenarioControl",
                                    "Change Label",
                                    QObject::tr("Change current objects label")},
                m_newName{newName},
                m_path{std::move(path)}
            {
                auto obj = m_path.find<T>();
                m_oldName = obj->metadata.name();
            }

                virtual void undo() override
            {
                auto obj = m_path.find<T>();
                obj->metadata.setName(m_oldName);
            }
                virtual void redo() override
            {
                auto obj = m_path.find<T>();
                obj->metadata.setName(m_newName);
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
                s << m_path << m_oldName << m_newName;
            }

                virtual void deserializeImpl(QDataStream& s) override
            {
                s >> m_path >> m_oldName >> m_newName;
            }

            private:
                ObjectPath m_path{};
                QString m_oldName{};
                QString m_newName;
        };
    }
}
