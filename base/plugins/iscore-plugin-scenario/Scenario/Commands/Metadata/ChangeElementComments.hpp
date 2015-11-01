#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementComments : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.
            public:
                static const char * factoryName()
                {
                    return ScenarioCommandFactoryName();
                }
                static const char * commandName()
                {
                    static QByteArray name = QString{"ChangeElementComments_%1"}.arg(T::staticMetaObject.className()).toUtf8();
                    return name.constData();
                }
                static QString description()
                {
                    return QObject::tr("Change %1 comments").arg(T::prettyName());
                }

                ChangeElementComments():
                    SerializableCommand {factoryName(),
                                         commandName(),
                                         description()}
                { }

                ChangeElementComments(Path<T>&& path, QString newComments) :
                    SerializableCommand {factoryName(),
                                         commandName(),
                                         description()},
                    m_path{std::move(path)},
                    m_newComments {newComments}
                {
                    auto& obj = m_path.find();
                    m_oldComments = obj.metadata.comment();
                }

                void undo() const override
                {
                    auto& obj = m_path.find();
                    obj.metadata.setComment(m_oldComments);
                }

                void redo() const override
                {
                    auto& obj = m_path.find();
                    obj.metadata.setComment(m_newComments);
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
                Path<T> m_path;
                QString m_oldComments;
                QString m_newComments;
        };
    }
}
