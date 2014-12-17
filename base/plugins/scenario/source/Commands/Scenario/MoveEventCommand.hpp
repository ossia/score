#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

struct EventData;

class MoveEventCommand : public iscore::SerializableCommand
{
	public:
        MoveEventCommand(ObjectPath &&scenarioPath, EventData data);
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

		double m_heightPosition{};
		double m_oldHeightPosition{};

        int m_deltaX{};
};
