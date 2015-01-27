#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

#include <core/tools/ObjectPath.hpp>

namespace Scenario
{
	namespace Command
	{
		/**
 * @brief The RemoveEvent class
 *
 * remove an event
 *
 */
        class RemoveEvent : public iscore::SerializableCommand
		{
			public:
                RemoveEvent();
                RemoveEvent(ObjectPath&& path);
				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;

				QVector<QByteArray> m_serializedStates;
				//		QByteArray m_serializedEvent;
				//		QVector<QByteArray> m_serializedConstraints; // The handlers inside the events are IN the constraints / Boxes / etc.
		};
	}
}
