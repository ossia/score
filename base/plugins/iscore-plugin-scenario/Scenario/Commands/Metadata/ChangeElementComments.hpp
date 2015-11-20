#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementComments final : public iscore::SerializableCommand
        {
                // No ISCORE_COMMAND here since it's a template.
            public:
                const CommandParentFactoryKey& parentKey() const override
                {
                    return ScenarioCommandFactoryName();
                }
                static const CommandFactoryKey& static_key()
                {
                    static const QByteArray name = QString{"ChangeElementComments_%1"}.arg(T::staticMetaObject.className()).toUtf8();
                    static const CommandFactoryKey kagi{name.constData()};
                    return kagi;
                }
                const CommandFactoryKey& key() const override
                {
                    return static_key();
                }
                QString description() const override
                {
                    return QObject::tr("Change %1 comments").arg(T::description());
                }

                ChangeElementComments() = default;

                ChangeElementComments(Path<T>&& path, QString newComments) :
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
                void serializeImpl(DataStreamInput& s) const override
                {
                    s << m_path << m_oldComments << m_newComments;
                }

                void deserializeImpl(DataStreamOutput& s) override
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
