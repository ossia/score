#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

#include <core/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementLabel : public iscore::SerializableCommand
        {

            public:
                ChangeElementLabel(ObjectPath&& path, QString newLabel) :
                    SerializableCommand {"ScenarioControl",
                                         QString{"ChangeElementLabel_%1"}.arg(T::className()),
                                         QObject::tr("Change current objects label")},
                    m_path {std::move(path) },
                    m_newLabel {newLabel}
                {
                    auto obj = m_path.find<T>();
                    m_oldLabel = obj->metadata.label();
                }

                virtual void undo() override
                {
                    auto obj = m_path.find<T>();
                    obj->metadata.setLabel(m_oldLabel);
                }

                virtual void redo() override
                {
                    auto obj = m_path.find<T>();
                    obj->metadata.setLabel(m_newLabel);
                }

                virtual int id() const override
                {
                    return -1;
                }

                virtual bool mergeWith(const QUndoCommand* other) override
                {
                    return false;
                }

            protected:
                virtual void serializeImpl(QDataStream& s) const override
                {
                    s << m_path << m_oldLabel << m_newLabel;
                }

                virtual void deserializeImpl(QDataStream& s) override
                {
                    s >> m_path >> m_oldLabel >> m_newLabel;
                }

            private:
                ObjectPath m_path;
                QString m_newLabel;
                QString m_oldLabel;
        };
    }
}
