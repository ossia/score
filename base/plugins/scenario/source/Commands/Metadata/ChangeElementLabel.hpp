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
                SerializableCommand{"ScenarioControl",
                                    "Change Label",
                                    QObject::tr("Change current objects label")},
                m_newLabel{newLabel},
                m_path{std::move(path)}
            {

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
                return 1;
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
                ObjectPath m_path{};
                QString m_oldLabel{};
                QString m_newLabel;
		};
	}
}
