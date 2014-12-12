#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

struct EventData;

class CreateEventAfterEventCommand : public iscore::SerializableCommand
{
	public:
		CreateEventAfterEventCommand();
        CreateEventAfterEventCommand(ObjectPath&& scenarioPath, EventData data);
        virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_path;

		int m_intervalId{};

		int m_firstEventId{};
		int m_time{};
		double m_heightPosition{};
};
