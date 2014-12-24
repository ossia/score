#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

struct EventData;

namespace Scenario
{
	namespace Command
	{
		class MoveEvent : public iscore::SerializableCommand
		{
			public:
				MoveEvent(ObjectPath &&scenarioPath, EventData data);
				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_scenarioPath;
				int m_eventId{};

				double m_oldHeightPosition{};
				double m_newHeightPosition{};
				int m_oldX{};
				int m_newX{};
		};
	}
}