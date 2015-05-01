#pragma once
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementName : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.
            public:
                static const char * className()
                {
                    static QByteArray name = QString{"ChangeElementName_%1"}.arg(T::staticMetaObject.className()).toLatin1();
                    return name.constData();
                }
                static QString description()
                {
                    return QObject::tr("Change %1 name").arg(T::prettyName());
                }

                ISCORE_COMMAND_DEFAULT_CTOR(ChangeElementName, "ScenarioControl")
                ChangeElementName(ObjectPath&& path, QString newName) :
                    SerializableCommand{"ScenarioControl",
                                        className(),
                                        description()},
                    m_path {std::move(path) },
                    m_newName {newName}
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
                ObjectPath m_path;
                QString m_newName;
                QString m_oldName;
        };
    }
}
