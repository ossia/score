#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QPointF>

struct EventModelData;

class CreateEventCommand : public iscore::SerializableCommand
{
		friend class CreateEventAfterEventCommandTest;
	public:
		CreateEventCommand();
		CreateEventCommand(ObjectPath&& scenarioPath, int time, double heightPosition);
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_path;

		int m_createdConstraintId{};
		int m_createdEventId{};

		int m_time{};
		double m_heightPosition{};
};
