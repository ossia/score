#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

#include <core/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
        template<class T>
        class ChangeElementColor : public iscore::SerializableCommand
        {
            public:
                ChangeElementColor(ObjectPath&& path, QColor newLabel) :
                    SerializableCommand {"ScenarioControl",
                                         QString{"ChangeElementColor_%1"}.arg(T::className()),
                                         QObject::tr("Change current object color")},
                    m_path {std::move(path) },
                    m_newColor {newLabel}
                {
                    auto obj = m_path.find<T>();
                    m_oldColor = obj->metadata.color();
                }

                virtual void undo() override
                {
                    auto obj = m_path.find<T>();
                    obj->metadata.setColor(m_oldColor);
                }

                virtual void redo() override
                {
                    auto obj = m_path.find<T>();
                    obj->metadata.setColor(m_newColor);
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
                    s << m_path << m_oldColor << m_newColor;
                }

                virtual void deserializeImpl(QDataStream& s) override
                {
                    s >> m_path >> m_oldColor >> m_newColor;
                }

            private:
                ObjectPath m_path;
                QColor m_newColor;
                QColor m_oldColor;
        };
    }
}
