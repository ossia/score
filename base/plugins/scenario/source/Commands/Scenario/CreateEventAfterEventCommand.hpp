#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

struct EventData;

class CreateEventAfterEventCommand : public iscore::SerializableCommand
{
		friend class CreateEventAfterEventCommandTest;
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

		int m_createdIntervalId{};
		int m_createdEventId{};

		int m_firstEventId{};
		int m_time{};
		double m_heightPosition{};
};
